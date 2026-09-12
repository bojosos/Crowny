#include "cwepch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/BuiltInShaderCatalog.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/MaterialPreset.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Scene/SceneManager.h"
#include "Editor/MaterialEditing.h"
#include "Panels/MaterialParameterPresentation.h"

#include "Panels/EntityInspector.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ScriptComponentInspector.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "UI/Properties.h"
#include "UI/SelectionProperties.h"
#include "UI/UIUtils.h"

#include "Crowny/Import/AudioClipImporter.h"
#include "Crowny/Import/FontImporter.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Import/ScriptImporter.h"
#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Import/TextFileImporter.h"
#include "Crowny/Import/TextureImporter.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"

#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"

#include "Editor/EditorUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    namespace
    {
        ScriptTypeIdentity ResolveDroppedScriptIdentity(const FileEntry& entry, bool logAmbiguity)
        {
            const String typeName = entry.Filepath.stem().string();
            const String defaultNamespace = Editor::Get().GetProjectPath().filename().string();
            Application* application = Application::TryGet();
            const ManagedScripting* managed = application != nullptr ? application->GetRuntime().GetManagedScripting() : nullptr;
            if (managed == nullptr || !managed->IsStarted())
                return { GAME_ASSEMBLY, defaultNamespace, typeName };

            Vector<ScriptTypeIdentity> candidates;
            for (const ScriptTypeSchema& type : managed->GetScriptCatalog().Types)
            {
                if (type.Identity.Assembly == GAME_ASSEMBLY && type.Identity.TypeName == typeName)
                    candidates.push_back(type.Identity);
            }
            if (candidates.size() == 1u)
                return candidates.front();

            const auto defaultMatch = std::find_if(candidates.begin(), candidates.end(),
                                                   [&](const ScriptTypeIdentity& identity) { return identity.Namespace == defaultNamespace; });
            if (defaultMatch != candidates.end() && std::find_if(defaultMatch + 1, candidates.end(), [&](const ScriptTypeIdentity& identity) {
                                                        return identity.Namespace == defaultNamespace;
                                                    }) == candidates.end())
                return *defaultMatch;

            if (candidates.empty())
                return { GAME_ASSEMBLY, defaultNamespace, typeName };

            if (logAmbiguity)
            {
                CW_ENGINE_WARN("Cannot infer which managed type '{}' represents because multiple game scripts use that class name. "
                               "Add it from the component menu instead.",
                               typeName);
            }
            return {};
        }

        bool IsAttachableScript(const FileEntry& entry)
        {
            if (entry.Metadata == nullptr)
                return false;
            const Ref<CSharpScriptImportOptions> options = StaticRefCast<CSharpScriptImportOptions>(entry.Metadata->ImportOptions);
            return options == nullptr || !options->IsEditorScript;
        }

        template <typename Component> bool SelectionHasMissingComponent(const Vector<Entity>& entities)
        {
            return std::any_of(entities.begin(), entities.end(), [](Entity entity) { return entity && !entity.HasComponent<Component>(); });
        }
    } // namespace

    template <typename T> T* InspectorPanel::BeginImportInspector()
    {
        UI::BeginPropertyGrid();
        return static_cast<T*>(m_ImportOptions.get());
    }

    InspectorPanel::InspectorPanel(const String& name) : ImGuiPanel(name)
    {
        // Built-in shaders need stable identifiers before any project material resolves its shader reference.
        BuiltInShaderCatalog::EnsureRegistered();

        m_EntityInspector.RegisterComponent<TransformComponent>("Transform");

        // Rendering
        m_EntityInspector.PushComponentGroup("Rendering");
        m_EntityInspector.RegisterComponent<CameraComponent>("Camera");
        m_EntityInspector.RegisterComponent<LightComponent>("Light");
        m_EntityInspector.RegisterComponent<DecalComponent>("Decal");
        m_EntityInspector.RegisterComponent<MeshRendererComponent>("Mesh Filter");
        m_EntityInspector.RegisterComponent<AnimationComponent>("Animation");
        m_EntityInspector.RegisterComponent<TextComponent>("Text");
        m_EntityInspector.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
        m_EntityInspector.RegisterComponent<ProceduralMeshComponent>("Procedural Mesh", [this](Entity entity) {
            auto& comp = entity.GetComponent<ProceduralMeshComponent>();
            const Entity receiverEntities[] = { entity };
            const auto receiverProperties = InspectorSelection(receiverEntities, "Procedural Mesh").Components<ProceduralMeshComponent>();
            UI::Property("Receive decals", receiverProperties.Bind("ReceiveDecals", &ProceduralMeshComponent::ReceiveDecals));
            UI::Property("Decal layers", receiverProperties.Bind("DecalLayers", &ProceduralMeshComponent::DecalLayers));

            AssetHandle<Asset> graphAsset = static_asset_cast<Asset>(comp.Graph);
            if (UIUtils::AssetReference("Graph", graphAsset, AssetType::NodeGraph))
            {
                comp.Graph = static_asset_cast<NodeGraphAsset>(graphAsset);
                comp.NeedsEvaluation = true;
            }

            // Create a new graph on this component if it doesn't have one yet
            if (!comp.Graph)
            {
                if (ImGui::Button("Create Node Graph"))
                {
                    Ref<NodeGraph> graph = CreateRef<NodeGraph>();
                    graph->SetDomain(NodeGraph::Domain::Geometry);
                    graph->SetName(entity.GetName() + " Graph");
                    graph->AddNode(NodeRegistry::Get().Create("GeometryOutputNode"_sid));

                    Path path = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (entity.GetName() + " Graph.cwng"));
                    Ref<NodeGraphAsset> asset = CreateRef<NodeGraphAsset>(graph);
                    ProjectLibrary::Get().CreateEntry(asset, path);
                    ProjectLibrary::Get().Refresh(path);

                    auto libraryEntry = ProjectLibrary::Get().FindEntry(path);
                    if (libraryEntry && libraryEntry->Type == LibraryEntryType::File)
                    {
                        FileEntry* fileEntry = (FileEntry*)libraryEntry.get();
                        if (fileEntry->Metadata)
                        {
                            comp.Graph = static_asset_cast<NodeGraphAsset>(AssetManager::TryGet()->LoadFromUUID(fileEntry->Metadata->Uuid));
                            comp.NeedsEvaluation = true;
                        }
                    }
                }
                return;
            }

            // Show graph info
            if (comp.Graph && comp.Graph.IsLoaded())
            {
                Ref<NodeGraph> graph = comp.Graph->GetGraph();
                if (graph)
                {
                    ImGui::TextDisabled("Graph: %s", graph->GetName().c_str());
                    if (comp.CpuMeshData)
                        ImGui::TextDisabled("Verts: %u  Indices: %u", comp.CpuMeshData->GetVertexCount(), comp.CpuMeshData->GetIndexCount());
                    else
                        ImGui::TextDisabled("Not evaluated yet");

                    // Open in node editor
                    if (ImGui::Button("Open Node Editor") && m_OpenNodeEditorCallback)
                        m_OpenNodeEditorCallback(comp.Graph);

                    // Show graph inputs
                    const auto& inputs = graph->GetInputs();
                    if (!inputs.empty())
                    {
                        ImGui::Text("Graph Inputs:");
                        for (const auto& input : inputs)
                        {
                            PinValue& val = comp.InputValues[input.ID];
                            // If it's a new input, initialize with default
                            if (comp.InputValues.find(input.ID) == comp.InputValues.end())
                                val = input.DefaultValue;

                            const UUID::TextBuffer inputId = input.ID.ToTextBuffer();
                            ImGui::PushID(inputId.data(), inputId.data() + UUID::TextLength);
                            const char* label = input.Name.c_str();

                            switch (input.DataType)
                            {
                            case PinDataType::Float: {
                                float v = std::holds_alternative<float>(val)
                                            ? std::get<float>(val)
                                            : (std::holds_alternative<float>(input.DefaultValue) ? std::get<float>(input.DefaultValue) : 0.0f);
                                if (ImGui::DragFloat(label, &v, 0.01f))
                                {
                                    val = v;
                                    comp.NeedsEvaluation = true;
                                }
                                break;
                            }
                            case PinDataType::Int: {
                                int32_t v = std::holds_alternative<int32_t>(val)
                                              ? std::get<int32_t>(val)
                                              : (std::holds_alternative<int32_t>(input.DefaultValue) ? std::get<int32_t>(input.DefaultValue) : 0);
                                if (ImGui::DragInt(label, &v))
                                {
                                    val = v;
                                    comp.NeedsEvaluation = true;
                                }
                                break;
                            }
                            case PinDataType::Vec3: {
                                glm::vec3 v = std::holds_alternative<glm::vec3>(val)
                                                ? std::get<glm::vec3>(val)
                                                : (std::holds_alternative<glm::vec3>(input.DefaultValue) ? std::get<glm::vec3>(input.DefaultValue)
                                                                                                         : glm::vec3(0.0f));
                                if (ImGui::DragFloat3(label, &v.x, 0.01f))
                                {
                                    val = v;
                                    comp.NeedsEvaluation = true;
                                }
                                break;
                            }
                            case PinDataType::Bool: {
                                bool v = std::holds_alternative<bool>(val)
                                           ? std::get<bool>(val)
                                           : (std::holds_alternative<bool>(input.DefaultValue) ? std::get<bool>(input.DefaultValue) : false);
                                if (ImGui::Checkbox(label, &v))
                                {
                                    val = v;
                                    comp.NeedsEvaluation = true;
                                }
                                break;
                            }
                            default:
                                break;
                            }
                            ImGui::PopID();
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Graph asset loaded but graph is null.");
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Evaluate"))
            {
                comp.NeedsEvaluation = true;
            }
        });
        m_EntityInspector.PopComponentGroup();

        // Physics
        m_EntityInspector.PushComponentGroup("Physics");
        m_EntityInspector.RegisterComponent<Rigidbody2DComponent>("Rigidbody 2D");
        m_EntityInspector.RegisterComponent<BoxCollider2DComponent>("Box Collider 2D");
        m_EntityInspector.RegisterComponent<CircleCollider2DComponent>("Circle Collider 2D");
        m_EntityInspector.RegisterComponent<Rigidbody3DComponent>("Rigidbody 3D");
        m_EntityInspector.RegisterComponent<BoxCollider3DComponent>("Box Collider 3D");
        m_EntityInspector.RegisterComponent<SphereCollider3DComponent>("Sphere Collider 3D");
        m_EntityInspector.RegisterComponent<CapsuleCollider3DComponent>("Capsule Collider 3D");
        m_EntityInspector.RegisterComponent<MeshCollider3DComponent>("Mesh Collider 3D");
        m_EntityInspector.PopComponentGroup();

        // Audio
        m_EntityInspector.PushComponentGroup("Audio");
        m_EntityInspector.RegisterComponent<AudioListenerComponent>("Audio Listener");
        m_EntityInspector.RegisterComponent<AudioSourceComponent>("Audio Source");
        m_EntityInspector.PopComponentGroup();

        // Scripting
        m_EntityInspector.RegisterComponent<ManagedScriptComponent>("C# Script");
    }

    InspectorPanel::~InspectorPanel()
    {
        ResetMaterialPreview();
        ResetAssetUndoTransactions(true);
        FlushPendingAssetSaves();
    }

    void InspectorPanel::HandleInspectorDragDrop(const Vector<Entity>& selectedEntities)
    {
        if (ImGui::BeginDragDropTarget()) // Add components when files are dropped on entities in the inspector (C#
                                          // script, AudioSource, etc...)
        {
            auto hasComponentCallback = [&](const FileEntry* entry) {
                if (entry == nullptr || entry->Metadata == nullptr)
                    return false;
                switch (entry->Metadata->Type)
                {
                case AssetType::AudioClip:
                    return SelectionHasMissingComponent<AudioSourceComponent>(selectedEntities);
                case AssetType::Mesh:
                    return SelectionHasMissingComponent<MeshRendererComponent>(selectedEntities);
                case AssetType::ScriptCode: {
                    if (!IsAttachableScript(*entry))
                        return false;
                    const ScriptTypeIdentity identity = ResolveDroppedScriptIdentity(*entry, false);
                    return !identity.IsValid() || std::any_of(selectedEntities.begin(), selectedEntities.end(), [&](Entity entity) {
                        Scene* scene = entity ? entity.GetScene() : nullptr;
                        return scene != nullptr && !scene->HasScriptComponent(entity, identity);
                    });
                }
                case AssetType::Texture:
                    return SelectionHasMissingComponent<SpriteRendererComponent>(selectedEntities);
                case AssetType::Font:
                    return SelectionHasMissingComponent<TextComponent>(selectedEntities);
                default:
                    return false;
                }
            };
            if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(hasComponentCallback))
            {
                if (fileEntry->Metadata)
                {
                    switch (fileEntry->Metadata->Type)
                    {
                    case AssetType::AudioClip: {
                        const AssetHandle<AudioClip> clip = static_asset_cast<AudioClip>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change = AddComponentToSelection<AudioSourceComponent>(
                          selectedEntities, [&](Entity, AudioSourceComponent& component) { component.SetClip(clip); });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    }
                    case AssetType::ScriptCode: {
                        const ScriptTypeIdentity identity = ResolveDroppedScriptIdentity(*fileEntry, true);
                        const SelectionComponentChange change = AddManagedScriptToSelection(selectedEntities, identity);
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    }
                    case AssetType::Font: {
                        const AssetHandle<Font> font = static_asset_cast<Font>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change =
                          AddComponentToSelection<TextComponent>(selectedEntities, [&](Entity, TextComponent& component) { component.Font = font; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    }
                    case AssetType::Mesh: {
                        const AssetHandle<Mesh> mesh = static_asset_cast<Mesh>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change = AddComponentToSelection<MeshRendererComponent>(
                          selectedEntities, [&](Entity, MeshRendererComponent& component) { component.MeshHandle = mesh; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    }
                    case AssetType::Texture: {
                        const AssetHandle<Texture> texture = static_asset_cast<Texture>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change = AddComponentToSelection<SpriteRendererComponent>(
                          selectedEntities, [&](Entity, SpriteRendererComponent& component) { component.Texture = texture; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void InspectorPanel::Render()
    {
        SaveReadyAssets();
        if (!BeginPanel())
        {
            EndPanel();
            return;
        }

        auto activeScene = SceneManager::TryGet()->GetActiveScene();
        if (!activeScene)
        {
            EndPanel();
            return;
        }

        m_InspectedEntities.erase(std::remove_if(m_InspectedEntities.begin(), m_InspectedEntities.end(),
                                                 [&](Entity entity) { return !entity.IsValid() || entity.GetScene() != activeScene.get(); }),
                                  m_InspectedEntities.end());
        if (!m_InspectedEntity.IsValid() || m_InspectedEntity.GetScene() != activeScene.get() ||
            std::find(m_InspectedEntities.begin(), m_InspectedEntities.end(), m_InspectedEntity) == m_InspectedEntities.end())
            m_InspectedEntity = m_InspectedEntities.empty() ? Entity{} : m_InspectedEntities.back();

        ImGui::BeginChild("InspectorChild");
        DrawHeader();

        switch (m_InspectorMode)
        {
        case InspectorMode::GameObject:
            m_EntityInspector.Render(m_InspectedEntity, m_InspectedEntities);
            break;
        case InspectorMode::Material:
            RenderMaterialInspector();
            break;
        case InspectorMode::PhysicsMaterial:
            RenderPhysicsMaterialInspector();
            break;
        case InspectorMode::AudioClipImport:
            if (m_ImportOptions)
                RenderAudioClipImportInspector();
            break;
        case InspectorMode::FontImport:
            if (m_ImportOptions)
                RenderFontImportInspector();
            break;
        case InspectorMode::ScriptImport:
            if (m_ImportOptions)
                RenderScriptImportInspector();
            break;
        case InspectorMode::ShaderImport:
            if (m_ImportOptions)
                RenderShaderImportInspector();
            break;
        case InspectorMode::MeshImport:
            if (m_ImportOptions)
                RenderMeshImportInspector();
            break;
        case InspectorMode::Prefab:
            if (m_ImportOptions)
                RenderPrefabInspector();
            break;
        case InspectorMode::TextureImport:
            if (m_ImportOptions)
                RenderTextureImportInspector();
            break;
        case InspectorMode::TextImport:
            if (m_ImportOptions)
                RenderTextImportInspector();
            break;
        case InspectorMode::Default:
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextDisabled("Select an entity or asset to inspect it.");
            break;
        }

        SaveReadyAssets();

        ImGui::EndChild();
        if (m_InspectorMode == InspectorMode::GameObject && m_InspectedEntity)
            HandleInspectorDragDrop(m_InspectedEntities);
        EndPanel();
    }

    void InspectorPanel::ResetMaterialPreview()
    {
        for (const auto& texture : m_MaterialThumbnails)
            ImGuiVulkanTexture::Release(texture);
        m_MaterialThumbnails.clear();
        ImGuiVulkanTexture::Release(m_MaterialPreviewImage);
        m_MaterialPreviewImage = nullptr;
        m_MaterialPreview.reset();
        m_MaterialDefaults = nullptr;
    }

    void InspectorPanel::DrawMaterialPreview(const AssetHandle<Material>& material)
    {
        if (!material->GetShader() || !ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##PreviewShape", &m_PreviewShape, "Sphere\0Cube\0"))
            ResetMaterialPreview();
        UI::SetTooltip("Choose the shape used to preview this material.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset view"))
        {
            m_PreviewYaw = 0.4f;
            m_PreviewPitch = 0.2f;
            m_PreviewDistance = 1.8f;
            m_PreviewViewChanged = true;
        }
        UI::SetTooltip("Restore the preview camera. Material parameters are unchanged.");

        if (!m_MaterialPreview || m_PreviewMaterialId != material.GetUUID() || m_PreviewLayout != material->GetLayoutVersion())
        {
            ResetMaterialPreview();
            const Ref<Mesh> mesh = m_PreviewShape == 0 ? MeshFactory::CreateSphere(0.5f, 48, 24) : MeshFactory::CreateCube(0.7f);
            const auto meshHandle = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(mesh));
            m_MaterialPreview = CreateScope<PreviewMaterialRenderer>(material, meshHandle);
            m_MaterialPreviewImage = nullptr;
            m_MaterialPreview->Setup(256, 256);
            m_PreviewMaterialId = material.GetUUID();
            m_PreviewLayout = material->GetLayoutVersion();
            m_PreviewViewChanged = true;
        }
        const double now = ImGui::GetTime();
        if ((m_PreviewViewChanged || m_PreviewParameters != material->GetParamVersion()) &&
            (m_PreviewViewChanged || now - m_LastPreviewRender >= 1.0 / 15.0))
        {
            m_MaterialPreview->SetView(m_PreviewYaw, m_PreviewPitch, m_PreviewDistance);
            m_MaterialPreviewImage = m_MaterialPreview->RenderPreview();
            m_PreviewParameters = material->GetParamVersion();
            m_PreviewViewChanged = false;
            m_LastPreviewRender = now;
        }
        const float size = std::max(64.0f, std::min(256.0f, ImGui::GetContentRegionAvail().x));
        if (m_MaterialPreviewImage)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - size) * 0.5f));
            const ImVec2 previewPosition = ImGui::GetCursorScreenPos();
            ImGui::Image(ImGuiVulkanTexture::Get(m_MaterialPreviewImage), ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SetCursorScreenPos(previewPosition);
            ImGui::InvisibleButton("##MaterialPreviewOrbit", ImVec2(size, size));
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
            UI::SetTooltip("Drag to rotate the preview. Scroll to zoom. Changes here affect only the preview camera.");
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
            {
                m_PreviewDistance = glm::clamp(m_PreviewDistance - ImGui::GetIO().MouseWheel * 0.15f, 1.1f, 4.0f);
                m_PreviewViewChanged = true;
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                m_PreviewYaw -= ImGui::GetIO().MouseDelta.x * 0.01f;
                m_PreviewPitch = glm::clamp(m_PreviewPitch - ImGui::GetIO().MouseDelta.y * 0.01f, -1.4f, 1.4f);
                m_PreviewViewChanged = true;
            }
        }
        else
            ImGui::TextDisabled("Preview unavailable for this material.");
    }

    void InspectorPanel::RenderMaterialInspector()
    {
        AssetHandle<Material> mat = static_asset_cast<Material>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
        if (!mat)
        {
            ResetMaterialUndoTransaction(false);
            ResetMaterialPreview();
            ImGui::TextWrapped("The material is not available yet. Check the import errors in the Console.");
            return;
        }

        DrawMaterialPreview(mat);

        const Ref<Asset> inspectedAsset = StaticRefCast<Asset>(mat.GetInternalPtr());
        UndoRedo& undoRedo = UndoRedo::Get();
        if (undoRedo.BeginComponentScope(m_MaterialUndo))
            m_MaterialUndo->Capture(m_InspectedAssetPath, mat.GetInternalPtr(), m_AssetSaveTracker);
        const auto applyEdit = [this, &inspectedAsset](bool changed, auto&& apply) {
            if (changed)
                apply();
            ObserveAssetEdit(inspectedAsset, changed);
        };

        UI::BeginPropertyGrid();

        // Shader selector: built-in engine shaders and shaders imported into the project.
        {
            UI::ScopedPropertyTooltip tooltip("Choose a shader. Compatible parameters and textures are retained when switching.");
            applyEdit(DrawMaterialShaderPicker(*mat), []() {});
        }

        if (!mat->GetShader())
        {
            ResetMaterialPreview();
            UI::EndPropertyGrid();
            ImGui::TextDisabled("This material has no shader. Pick one above to edit its parameters.");
            undoRedo.EndComponentScope();
            SaveReadyAssets();
            return;
        }

        int32_t alphaMode = mat->HasAlphaModeOverride() ? static_cast<int32_t>(mat->GetAlphaMode()) + 1 : 0;
        const bool alphaModeChanged =
          UI::PropertyDropdown("Alpha Mode", { "Inferred (Shader)", "Opaque", "Alpha Mask", "Premultiplied", "Additive", "Weighted OIT" }, alphaMode);
        applyEdit(alphaModeChanged, [&]() {
            if (alphaMode == 0)
                mat->ClearAlphaModeOverride();
            else
                mat->SetAlphaMode(static_cast<AlphaMode>(alphaMode - 1));
        });

        applyEdit(DrawMaterialPresetRow(*mat), []() {});
        if (mat->GetDomain() == MaterialDomain::Decal)
        {
            const char* channels[] = { "Base color",        "Normal",   "Roughness",   "Metallic",
                                       "Ambient occlusion", "Emission", "Corrections", "Opaque coating" };
            for (uint32_t channel = 0; channel < 8; ++channel)
            {
                int32_t mask = mat->GetDataParam<int32_t>("decalChannels");
                bool enabled = (mask & (1 << channel)) != 0;
                applyEdit(UI::Property(channels[channel], enabled),
                          [&]() { mat->SetInt("decalChannels", enabled ? mask | (1 << channel) : mask & ~(1 << channel)); });
                if (enabled)
                {
                    const char* parameter = channel < 4 ? "decalStrengths" : "decalStrengths2";
                    glm::vec4 strengths = mat->GetDataParam<glm::vec4>(parameter);
                    String label = String(channels[channel]) + " strength";
                    applyEdit(UI::Property(label.c_str(), strengths[channel % 4], 0.01f, 0.0f, 1.0f), [&]() { mat->SetColor(parameter, strengths); });
                }
            }
        }
        if (mat->GetDomain() == MaterialDomain::Surface)
        {
            bool supported = false;
            for (uint32_t pass = 0; pass < mat->GetPassCount(); ++pass)
                if (const auto pipeline = mat->GetGraphicsPipeline(pass))
                    supported |= pipeline->GetParamInfo()->HasBinding(UniformParamInfo::ParamType::Buffer, 2, 1);
            if (!supported)
                ImGui::TextWrapped("This surface shader has no decal response interface.");
            const char* responses[] = { "Decal color", "Decal normal",   "Decal roughness",   "Decal metallic",
                                        "Decal AO",    "Decal emission", "Decal corrections", "Decal coating" };
            for (uint32_t channel = 0; channel < 8; ++channel)
            {
                const uint32_t mask = mat->GetDecalResponseMask();
                bool enabled = (mask & (1u << channel)) != 0;
                applyEdit(UI::Property(responses[channel], enabled),
                          [&]() { mat->SetDecalResponseMask(enabled ? mask | (1u << channel) : mask & ~(1u << channel)); });
            }
        }

        UI::EndPropertyGrid();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::PushID("MaterialParameterSearch");
        UIUtils::SearchWidget(m_MaterialParameterSearch, "Search parameters...");
        ImGui::PopID();
        if (!m_MaterialDefaults || m_DefaultsLayout != mat->GetLayoutVersion())
        {
            m_MaterialDefaults = Material::Create(mat->GetShader());
            m_MaterialDefaults->ApplyModelDefaults();
            m_DefaultsLayout = mat->GetLayoutVersion();
        }
        const Vector<ShaderParameterDesc>& params = m_MaterialSchemaCache.Resolve(*mat);
        Vector<Ref<Texture>> visibleThumbnails;
        size_t visibleCount = 0;
        for (const char* group : { "Surface", "Toon shading", "Emission", "Transparency", "Outline", "Textures" })
        {
            const auto matches = [&](const ShaderParameterDesc& parameter) {
                // Alpha has a typed control above; environment use is supplied by the renderer.
                return parameter.Identifier != "alphaMode" && parameter.Identifier != "useIBL" && parameter.Identifier != "decalChannels" &&
                       parameter.Identifier != "decalStrengths" && parameter.Identifier != "decalStrengths2" &&
                       StringView(MaterialParameterGroup(parameter)) == group &&
                       (m_MaterialParameterSearch.empty() || StringUtils::IsSearchMathing(parameter.DisplayName, m_MaterialParameterSearch) ||
                        StringUtils::IsSearchMathing(parameter.Identifier, m_MaterialParameterSearch));
            };
            const size_t count = std::count_if(params.begin(), params.end(), matches);
            visibleCount += count;
            if (count == 0 || !ImGui::CollapsingHeader(group, ImGuiTreeNodeFlags_DefaultOpen))
                continue;
            ImGui::PushID(group);
            const bool resetGroup = ImGui::SmallButton("Reset group");
            undoRedo.OnItemInteract(resetGroup);
            UI::SetTooltip("Restore the shader defaults for the visible parameters in this group.");
            applyEdit(resetGroup, [&]() {
                for (const auto& parameter : params)
                    if (matches(parameter))
                        ResetMaterialParameter(*mat, *m_MaterialDefaults, parameter);
            });
            UI::BeginPropertyGrid();
            for (const auto& param : params)
            {
                if (!matches(param))
                    continue;
                ImGui::PushID(param.Identifier.c_str());
                const String help = MaterialParameterTooltip(param);
                UI::ScopedPropertyTooltip tooltip(help);
                const auto drawIntegerVector = [&](auto value, auto setter) {
                    UI::Pre(param.DisplayName.c_str());
                    const bool changed = ImGui::DragScalarN("##Value", ImGuiDataType_S32, glm::value_ptr(value), value.length(), 1.0f);
                    undoRedo.OnItemInteract(changed);
                    UI::Post();
                    applyEdit(changed, [&]() { (mat.GetInternalPtr().get()->*setter)(param.Identifier, value); });
                };
                const auto drawMatrix = [&](auto value, auto setter) {
                    UI::Pre(param.DisplayName.c_str());
                    bool changed = false;
                    ImGui::BeginGroup();
                    for (int column = 0; column < value.length(); ++column)
                    {
                        ImGui::PushID(column);
                        const bool columnChanged =
                          ImGui::DragScalarN("##Column", ImGuiDataType_Float, glm::value_ptr(value[column]), value.length(), 0.01f);
                        undoRedo.OnItemInteract(columnChanged);
                        changed |= columnChanged;
                        ImGui::PopID();
                    }
                    ImGui::EndGroup();
                    UI::Post();
                    applyEdit(changed, [&]() { (mat.GetInternalPtr().get()->*setter)(param.Identifier, value); });
                };
                switch (param.Type)
                {
                case ShaderParamType::Int2:
                    drawIntegerVector(mat->GetDataParam<glm::ivec2>(param.Identifier), &Material::SetInt2);
                    break;
                case ShaderParamType::Int3:
                    drawIntegerVector(mat->GetDataParam<glm::ivec3>(param.Identifier), &Material::SetInt3);
                    break;
                case ShaderParamType::Int4:
                    drawIntegerVector(mat->GetDataParam<glm::ivec4>(param.Identifier), &Material::SetInt4);
                    break;
                case ShaderParamType::Mat3:
                    drawMatrix(mat->GetDataParam<glm::mat3>(param.Identifier), &Material::SetMat3);
                    break;
                case ShaderParamType::Mat4:
                    drawMatrix(mat->GetDataParam<glm::mat4>(param.Identifier),
                               static_cast<void (Material::*)(const String&, const glm::mat4&)>(&Material::SetMatrix));
                    break;
                case ShaderParamType::Float: {
                    float value = mat->GetDataParam<float>(param.Identifier);
                    bool modified = param.HasRange ? UI::PropertySlider(param.DisplayName.c_str(), value, param.RangeMin, param.RangeMax)
                                                   : UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetFloat(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Float2: {
                    glm::vec2 value = mat->GetDataParam<glm::vec2>(param.Identifier);
                    const bool modified = UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetFloat2(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Float3: {
                    glm::vec3 value = mat->GetDataParam<glm::vec3>(param.Identifier);
                    const bool modified = UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetVector3(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Float4: {
                    glm::vec4 value = mat->GetDataParam<glm::vec4>(param.Identifier);
                    const bool modified = UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetColor(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Color3: {
                    // Read as vec3, display with color picker
                    glm::vec3 value = mat->GetDataParam<glm::vec3>(param.Identifier);
                    ImGuiColorEditFlags flags = param.Flags.IsSet(ShaderParamFlag::HDR) ? ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float : 0;
                    const bool modified = UI::PropertyColor(param.DisplayName.c_str(), value, flags);
                    applyEdit(modified, [&]() { mat->SetVector3(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Color4: {
                    // Read as vec4, display with color picker
                    glm::vec4 value = mat->GetDataParam<glm::vec4>(param.Identifier);
                    ImGuiColorEditFlags flags = param.Flags.IsSet(ShaderParamFlag::HDR) ? ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float : 0;
                    const bool modified = UI::PropertyColor(param.DisplayName.c_str(), value, flags);
                    applyEdit(modified, [&]() { mat->SetColor(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Int: {
                    int value = mat->GetDataParam<int>(param.Identifier);
                    const bool modified = UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetInt(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Bool: {
                    bool value = mat->GetDataParam<bool>(param.Identifier);
                    const bool modified = UI::Property(param.DisplayName.c_str(), value);
                    applyEdit(modified, [&]() { mat->SetBool(param.Identifier, value); });
                    break;
                }
                case ShaderParamType::Texture2D:
                case ShaderParamType::Texture3D:
                case ShaderParamType::TextureCube: {
                    AssetHandle<Texture> texHandle = mat->GetTextureHandle(param.Identifier);
                    const bool modified = UIUtils::AssetSearch<Texture>(param.DisplayName, texHandle);
                    // The asset picker can finish on a popup item or drag target with no item ID.
                    if (modified)
                        undoRedo.OnItemInteract({ ImGui::GetID("##TextureAssignment"), false, false, false, true });
                    applyEdit(modified, [&]() { mat->SetTexture(param.Identifier, texHandle); });
                    if (param.Type == ShaderParamType::Texture2D)
                    {
                        const Ref<Texture> texture = mat->GetTexture(param.Set, param.Slot);
                        if (texture)
                        {
                            if (std::find(visibleThumbnails.begin(), visibleThumbnails.end(), texture) == visibleThumbnails.end())
                                visibleThumbnails.push_back(texture);
                            UI::Pre("");
                            ImGui::Image(ImGuiVulkanTexture::Get(texture), ImVec2(48, 48), ImVec2(0, 1), ImVec2(1, 0));
                            UI::Post();
                        }
                    }
                    break;
                }
                default:
                    break;
                }
                if (ImGui::BeginPopupContextItem("##ResetMaterialParameter"))
                {
                    const bool reset = ImGui::MenuItem("Reset to shader default");
                    undoRedo.OnItemInteract(reset);
                    applyEdit(reset, [&]() { ResetMaterialParameter(*mat, *m_MaterialDefaults, param); });
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            UI::EndPropertyGrid();
            ImGui::PopID();
        }
        if (visibleCount == 0)
            ImGui::TextDisabled("No matching parameters.");
        for (const auto& texture : m_MaterialThumbnails)
            if (std::find(visibleThumbnails.begin(), visibleThumbnails.end(), texture) == visibleThumbnails.end())
                ImGuiVulkanTexture::Release(texture);
        m_MaterialThumbnails = std::move(visibleThumbnails);
        undoRedo.EndComponentScope();
    }

    namespace
    {
        constexpr double MATERIAL_STATUS_SECONDS = 4.0;

        String DescribeShader(const AssetHandle<Shader>& shader, const Vector<MaterialShaderOption>& options)
        {
            if (!shader.HasUUID())
                return "None";
            for (const MaterialShaderOption& option : options)
            {
                if (option.Uuid == shader.GetUUID())
                    return option.Name;
            }
            if (ProjectLibrary* library = ProjectLibrary::TryGet())
            {
                const String name = library->GetAssetName(shader.GetUUID());
                if (!name.empty())
                    return name;
            }
            Path assetPath;
            if (AssetManager::TryGet() != nullptr && AssetManager::TryGet()->GetAssetPath(shader.GetUUID(), assetPath))
                return assetPath.stem().string();
            if (shader.IsLoaded() && !shader->GetName().empty())
                return shader->GetName();
            return shader.IsLoaded() ? String("Unnamed shader") : String("Missing shader");
        }

        bool DrawPickerSectionLabel(const char* label, bool& drawn)
        {
            if (drawn)
                return true;
            ImGui::TextDisabled("%s", label);
            drawn = true;
            return true;
        }
    } // namespace

    void InspectorPanel::RefreshMaterialShaderOptions()
    {
        AssetManager* assetManager = AssetManager::TryGet();
        if (!m_MaterialPicker.BuiltInShadersLoaded && assetManager != nullptr)
        {
            BuiltInShaderCatalog::EnsureRegistered();
            m_MaterialPicker.BuiltInShaders.clear();
            for (const BuiltInShaderEntry& entry : BuiltInShaderCatalog::Enumerate())
            {
                MaterialShaderOption option;
                option.Name = entry.AssetPath == Path(PBRIBL_SHADER_PATH) ? "PBR (Standard)" : entry.Name;
                option.Uuid = entry.Uuid;
                option.AssetPath = entry.AssetPath;
                option.BuiltIn = true;
                // Deserializing the shader asset is device free; it only reflects technique metadata.
                const AssetHandle<Shader> shader = assetManager->Load<Shader>(entry.AssetPath);
                option.MaterialCapable = shader && BuiltInShaderCatalog::IsMaterialShader(entry.AssetPath.generic_string(), *shader);
                m_MaterialPicker.BuiltInShaders.push_back(std::move(option));
            }
            m_MaterialPicker.BuiltInShadersLoaded = true;
        }

        m_MaterialPicker.ShaderOptions = m_MaterialPicker.BuiltInShaders;
        if (ProjectLibrary* library = ProjectLibrary::TryGet())
        {
            for (const UUID& uuid : library->GetAllAssets(AssetType::Shader))
            {
                MaterialShaderOption option;
                option.Name = library->GetAssetName(uuid);
                if (option.Name.empty())
                    continue;
                option.Uuid = uuid;
                option.BuiltIn = false;
                const AssetHandle<Shader> shader = assetManager->LoadFromUUID<Shader>(uuid);
                option.MaterialCapable = shader && BuiltInShaderCatalog::IsMaterialShader(shader->GetName(), *shader);
                m_MaterialPicker.ShaderOptions.push_back(std::move(option));
            }
        }
    }

    bool InspectorPanel::DrawMaterialShaderPicker(Material& material)
    {
        if (material.GetDomain() == MaterialDomain::Decal)
        {
            UI::Pre("Domain");
            ImGui::TextUnformatted("Decal");
            UI::Post();
            return false;
        }
        bool changed = false;
        const AssetHandle<Shader> currentShader = material.GetShader();

        UI::Pre("Shader");
        const ImVec2 originalAlign = ImGui::GetStyle().ButtonTextAlign;
        ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
        const String buttonText = DescribeShader(currentShader, m_MaterialPicker.ShaderOptions);
        if (ImGui::Button(UI::GenerateLabelID(buttonText), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            ImGui::OpenPopup("##MaterialShaderPicker");
        ImGui::GetStyle().ButtonTextAlign = originalAlign;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            if (currentShader.HasUUID())
                ImGui::SetTooltip("%s\nClick to choose a built-in or project shader.", buttonText.c_str());
            else
                ImGui::SetTooltip("Click to choose a built-in or project shader.");
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(AssetType::Shader))
            {
                const AssetHandle<Shader> dropped = static_asset_cast<Shader>(ProjectLibrary::Get().Load(fileEntry));
                if (dropped)
                {
                    changed = ChangeMaterialShader(material, dropped);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SetNextWindowSize(ImVec2(std::max(260.0f, ImGui::GetItemRectSize().x), 0.0f));
        if (UIUtils::BeginPopup("##MaterialShaderPicker", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            static bool grabFocus = false;
            if (ImGui::GetCurrentWindow()->Appearing)
            {
                RefreshMaterialShaderOptions();
                m_MaterialPicker.ShaderSearch.clear();
                grabFocus = true;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            UIUtils::SearchWidget(m_MaterialPicker.ShaderSearch, "Search shaders...", &grabFocus);
            ImGui::Checkbox("Show internal shaders", &m_MaterialPicker.ShowInternalShaders);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Engine shaders without a material model (compute, depth, post-process). Materials using them cannot be rendered.");

            const Vector<const MaterialShaderOption*> filtered =
              FilterMaterialShaderOptions(m_MaterialPicker.ShaderOptions, m_MaterialPicker.ShaderSearch, m_MaterialPicker.ShowInternalShaders);
            if (ImGui::BeginListBox("##MaterialShaderList", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 12.0f)))
            {
                bool builtInLabel = false;
                bool projectLabel = false;
                if (filtered.empty())
                    ImGui::TextDisabled("No shaders match.");
                for (const MaterialShaderOption* option : filtered)
                {
                    if (option->BuiltIn)
                        DrawPickerSectionLabel("Built-in", builtInLabel);
                    else
                        DrawPickerSectionLabel("Project", projectLabel);

                    const bool selected = currentShader.HasUUID() && currentShader.GetUUID() == option->Uuid;
                    String label = option->Name;
                    if (!option->MaterialCapable)
                        label += "  (internal)";
                    ImGui::PushID(option->Uuid.ToString().c_str());
                    ImGui::BeginDisabled(!option->MaterialCapable);
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        AssetHandle<Shader> picked;
                        if (AssetManager* assetManager = AssetManager::TryGet())
                            picked =
                              option->BuiltIn ? assetManager->Load<Shader>(option->AssetPath) : assetManager->LoadFromUUID<Shader>(option->Uuid);
                        if (picked)
                        {
                            UndoRedo::Get().OnItemInteract(true);
                            changed = ChangeMaterialShader(material, picked);
                        }
                        else
                        {
                            m_MaterialPicker.StatusMessage = "Failed to load shader '" + option->Name + "'.";
                            m_MaterialPicker.StatusIsError = true;
                            m_MaterialPicker.StatusExpiry = ImGui::GetTime() + MATERIAL_STATUS_SECONDS;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
            UIUtils::EndPopup();
        }
        UI::Post();
        return changed;
    }

    void InspectorPanel::RefreshMaterialPresetOptions()
    {
        Vector<MaterialPresetEntry> entries = MaterialPresetLibrary::EnumerateBuiltIn();
        ProjectLibrary* library = ProjectLibrary::TryGet();
        AssetManager* assetManager = AssetManager::TryGet();
        if (library != nullptr && assetManager != nullptr)
        {
            for (const UUID& uuid : library->GetAllAssets(AssetType::MaterialPreset))
            {
                const AssetHandle<MaterialPreset> preset = assetManager->LoadFromUUID<MaterialPreset>(uuid);
                if (!preset)
                    continue;
                MaterialPresetEntry entry;
                entry.Name = library->GetAssetName(uuid);
                if (entry.Name.empty())
                    entry.Name = preset->GetName();
                entry.Uuid = uuid;
                entry.SourcePath = library->UuidToPath(uuid);
                entry.BuiltIn = false;
                entry.Preset = preset.GetInternalPtr();
                entries.push_back(std::move(entry));
            }
        }
        m_MaterialPicker.PresetOptions = std::move(entries);
    }

    void InspectorPanel::SaveMaterialAsPreset(const Material& material)
    {
        ProjectLibrary* library = ProjectLibrary::TryGet();
        if (library == nullptr || m_InspectedAssetPath.empty())
            return;
        const String materialName = m_InspectedAssetPath.stem().string();
        const Path presetPath =
          EditorUtils::GetUniquePath(m_InspectedAssetPath.parent_path() / (materialName + " Preset" + MaterialPresetLibrary::PRESET_EXTENSION));
        const Ref<MaterialPreset> preset = MaterialPreset::CaptureFromMaterial(material, presetPath.stem().string());
        library->CreateEntry(preset, presetPath);
        const bool saved = fs::is_regular_file(presetPath);
        m_MaterialPicker.StatusMessage = saved ? "Saved preset '" + presetPath.filename().string() + "'." : "Failed to save the preset.";
        m_MaterialPicker.StatusIsError = !saved;
        m_MaterialPicker.StatusExpiry = ImGui::GetTime() + MATERIAL_STATUS_SECONDS;
    }

    bool InspectorPanel::DrawMaterialPresetRow(Material& material)
    {
        bool changed = false;
        UI::Pre("Preset");
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float saveWidth = ImGui::CalcTextSize("Save as...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float applyWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - saveWidth - spacing);
        if (ImGui::Button("Apply preset...", ImVec2(applyWidth, 0.0f)))
            ImGui::OpenPopup("##MaterialPresetPicker");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Apply a built-in or project preset. Texture assignments are kept.");
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Save as...", ImVec2(saveWidth, 0.0f)))
            SaveMaterialAsPreset(material);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Save the current parameter values as a .cwpreset next to this material.");

        ImGui::SetNextWindowSize(ImVec2(std::max(260.0f, applyWidth + saveWidth + spacing), 0.0f));
        if (UIUtils::BeginPopup("##MaterialPresetPicker", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            static bool grabFocus = false;
            if (ImGui::GetCurrentWindow()->Appearing)
            {
                RefreshMaterialPresetOptions();
                m_MaterialPicker.PresetOptions = MaterialPresetLibrary::FilterCompatible(material, std::move(m_MaterialPicker.PresetOptions));
                m_MaterialPicker.PresetSearch.clear();
                grabFocus = true;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            UIUtils::SearchWidget(m_MaterialPicker.PresetSearch, "Search presets...", &grabFocus);
            if (ImGui::BeginListBox("##MaterialPresetList", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 8.0f)))
            {
                bool builtInLabel = false;
                bool projectLabel = false;
                bool any = false;
                for (const MaterialPresetEntry& entry : m_MaterialPicker.PresetOptions)
                {
                    if (!m_MaterialPicker.PresetSearch.empty() && !StringUtils::IsSearchMathing(entry.Name, m_MaterialPicker.PresetSearch))
                        continue;
                    any = true;
                    if (entry.BuiltIn)
                        DrawPickerSectionLabel("Built-in", builtInLabel);
                    else
                        DrawPickerSectionLabel("Project", projectLabel);
                    ImGui::PushID(&entry);
                    if (ImGui::Selectable(entry.Name.c_str(), false))
                    {
                        UndoRedo::Get().OnItemInteract(true);
                        String error;
                        if (entry.Preset != nullptr && entry.Preset->Validate(material.GetBindings(), &error) && material.ApplyPreset(*entry.Preset))
                        {
                            changed = true;
                            m_MaterialPicker.StatusMessage = "Applied preset '" + entry.Name + "'.";
                            m_MaterialPicker.StatusIsError = false;
                        }
                        else
                        {
                            m_MaterialPicker.StatusMessage = "Preset '" + entry.Name + "' does not fit this shader. " + error;
                            m_MaterialPicker.StatusIsError = true;
                        }
                        m_MaterialPicker.StatusExpiry = ImGui::GetTime() + MATERIAL_STATUS_SECONDS;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                if (!any)
                    ImGui::TextDisabled("No compatible presets. Use 'Save as...' to create one.");
                ImGui::EndListBox();
            }
            UIUtils::EndPopup();
        }
        UI::Post();

        if (!m_MaterialPicker.StatusMessage.empty() && ImGui::GetTime() < m_MaterialPicker.StatusExpiry)
        {
            ImGui::NextColumn();
            const ImVec4 color = m_MaterialPicker.StatusIsError ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f) : ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", m_MaterialPicker.StatusMessage.c_str());
            ImGui::PopStyleColor();
            ImGui::NextColumn();
        }
        else if (!m_MaterialPicker.StatusMessage.empty())
            m_MaterialPicker.StatusMessage.clear();
        return changed;
    }

    void InspectorPanel::RenderPhysicsMaterialInspector()
    {
        AssetHandle<Asset> asset = ProjectLibrary::Get().Load(m_InspectedAssetPath);
        if (!asset)
        {
            ResetAssetUndoTransactions(false);
            ImGui::TextDisabled("The physics material could not be loaded.");
            return;
        }

        const Ref<Asset> inspectedAsset = asset.GetInternalPtr();
        UndoRedo& undoRedo = UndoRedo::Get();
        if (undoRedo.BeginComponentScope(m_PhysicsMaterialUndo))
            m_PhysicsMaterialUndo->Capture(m_InspectedAssetPath, inspectedAsset, m_AssetSaveTracker);
        const auto applyEdit = [this, &inspectedAsset](bool changed, auto&& apply) {
            if (changed)
                apply();
            ObserveAssetEdit(inspectedAsset, changed);
        };
        const auto drawMaterial = [&](auto material) {
            if (!material)
                return;
            float density = material->GetDensity();
            float friction = material->GetFriction();
            float restitution = material->GetRestitution();
            float threshold = material->GetRestitutionThreshold();
            PhysicsCombineMode frictionCombine = material->GetFrictionCombine();
            PhysicsCombineMode restitutionCombine = material->GetRestitutionCombine();

            UI::BeginPropertyGrid();
            applyEdit(UI::Property("Density", density, 0.05f, 0.0f, 0.0f), [&]() { material->SetDensity(density); });
            applyEdit(UI::Property("Friction", friction, 0.05f, 0.0f, 0.0f), [&]() { material->SetFriction(friction); });
            applyEdit(UI::Property("Restitution", restitution, 0.05f, 0.0f, 1.0f), [&]() { material->SetRestitution(restitution); });
            applyEdit(UI::Property("Restitution Threshold", threshold, 0.05f, 0.0f, 0.0f), [&]() { material->SetRestitutionThreshold(threshold); });
            applyEdit(UI::PropertyDropdown("Friction Combine", { "Geometric Mean", "Average", "Minimum", "Multiply", "Maximum" }, frictionCombine),
                      [&]() { material->SetFrictionCombine(frictionCombine); });
            const bool restitutionCombineChanged =
              UI::PropertyDropdown("Restitution Combine", { "Geometric Mean", "Average", "Minimum", "Multiply", "Maximum" }, restitutionCombine);
            applyEdit(restitutionCombineChanged, [&]() { material->SetRestitutionCombine(restitutionCombine); });
            UI::EndPropertyGrid();
        };

        if (asset->GetAssetType() == AssetType::PhysicsMaterial2D)
            drawMaterial(static_asset_cast<PhysicsMaterial2D>(asset));
        else if (asset->GetAssetType() == AssetType::PhysicsMaterial)
            drawMaterial(static_asset_cast<PhysicsMaterial3D>(asset));
        else
            ImGui::TextDisabled("The selected asset is not a physics material.");

        undoRedo.EndComponentScope();
    }

    void InspectorPanel::ObserveAssetEdit(const Ref<Asset>& asset, bool changed)
    {
        m_AssetSaveTracker->Observe(m_InspectedAssetPath, asset, changed, ImGui::IsItemActive(), ImGui::IsItemDeactivatedAfterEdit());
    }

    void InspectorPanel::SaveReadyAssets()
    {
        ProjectLibrary* library = ProjectLibrary::TryGet();
        if (library == nullptr)
            return;

        while (const std::optional<AssetSaveRequest> request = m_AssetSaveTracker->TakeReady())
        {
            const bool saved = library->SaveEntry(request->Value, request->Filepath);
            m_AssetSaveTracker->Resolve(request->Filepath, saved);
        }
    }

    void InspectorPanel::FlushPendingAssetSaves()
    {
        m_AssetSaveTracker->Flush();
        SaveReadyAssets();
    }

    void InspectorPanel::ResetAssetUndoTransactions(bool finishInteraction)
    {
        ResetMaterialUndoTransaction(finishInteraction);
        if (UndoRedo* undoRedo = UndoRedo::TryGet())
        {
            if (finishInteraction)
                undoRedo->FinishComponentScope(m_PhysicsMaterialUndo);
            else
                undoRedo->CancelComponentScope(m_PhysicsMaterialUndo);
        }
        m_PhysicsMaterialUndo->Reset();
    }

    void InspectorPanel::ResetMaterialUndoTransaction(bool finishInteraction)
    {
        if (UndoRedo* undoRedo = UndoRedo::TryGet())
        {
            if (finishInteraction)
                undoRedo->FinishComponentScope(m_MaterialUndo);
            else
                undoRedo->CancelComponentScope(m_MaterialUndo);
        }
        m_MaterialUndo->Reset();
    }

    void InspectorPanel::RenderAudioClipImportInspector()
    {
        auto* opts = BeginImportInspector<AudioClipImportOptions>();

        m_HasPropertyChanged |= UI::PropertyDropdown("Format", { "PCM", "Vorbis" }, opts->Format);
        m_HasPropertyChanged |= UI::PropertyDropdown("Load Mode", { "Load Decompressed", "Load Compressed", "Stream" }, opts->ReadMode);

        uint32_t bitDepth = opts->BitDepth / 8 - 1;
        m_HasPropertyChanged |= UI::PropertyDropdown("Audio Bit Depth", { "8", "16", "24", "32" }, bitDepth);
        opts->BitDepth = (bitDepth + 1) * 8;

        m_HasPropertyChanged |= UI::Property("3D", opts->Is3D);

        EndImportInspector(0, ImGui::GetColumnWidth());

        // Footer
        const float yPos = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
        ImGui::SetCursorPosY(yPos);
        if (ImGui::Button("Play"))
        {
            if (m_HasPropertyChanged) // Why did I do this?
                ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions, true);
            AssetHandle<AudioClip> clip = static_asset_cast<AudioClip>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
            AudioManager::TryGet()->StopManualSources();
            AudioManager::TryGet()->Play("Inspector", clip);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            AudioManager::TryGet()->StopManualSources();
        ImGui::SameLine();
        const float progress = AudioManager::TryGet()->GetGlobalSourceProgress("Inspector");
        ImGui::ProgressBar(progress);
    }

    void InspectorPanel::RenderFontImportInspector()
    {
        auto* opts = BeginImportInspector<FontImportOptions>();

        int dropdownIdx = opts->AutomaticFontSampling ? 0 : 1;
        if (UI::PropertyDropdown("Sampling Point Size", { "Automatic", "Custom Size" }, dropdownIdx))
        {
            m_HasPropertyChanged = true;
            opts->AutomaticFontSampling = dropdownIdx == 1 ? false : true;
        }
        if (!opts->AutomaticFontSampling || opts->AutoSizeAtlas)
            m_HasPropertyChanged |= UI::Property("Sampling Size", opts->SamplingFontSize, 4U, 512U);

        if (opts->DynamicFontAtlas)
        {
            opts->DynamicFontAtlas = false;
            m_HasPropertyChanged = true;
        }
        uint32_t atlasMode = 0;
        {
            UI::ScopedDisable disableAtlasMode(true);
            UI::PropertyDropdown("Atlas Mode", { "Static MSDF" }, atlasMode);
        }
        UI::SetTooltip("Crowny currently imports static MSDF atlases. Runtime-populated dynamic atlases are rejected by the importer.");

        m_HasPropertyChanged |= UI::Property("Auto size atlas", opts->AutoSizeAtlas);

        if (opts->AutoSizeAtlas)
        {
            m_HasPropertyChanged |= UI::PropertyDropdown(
              "Dimension Constraints", { "Power of Two Square", "Power of Two Rectangle", "Multiple of Four Square", "Even Square", "Square" },
              opts->AtlasDimensionsConstraint);
        }
        else
        {
            Vector<String> atlasSizeUIValues = { "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384" };
            auto findSizeIdx = [&atlasSizeUIValues](uint32_t size) -> uint32_t {
                const String value = std::to_string(size);
                const auto iter = std::find(atlasSizeUIValues.begin(), atlasSizeUIValues.end(), value);
                return iter == atlasSizeUIValues.end() ? 8U : static_cast<uint32_t>(std::distance(atlasSizeUIValues.begin(), iter));
            };

            uint32_t widthIdx = findSizeIdx(opts->AtlasWidth);
            if (UI::PropertyDropdown("Atlas Width", atlasSizeUIValues, widthIdx))
            {
                opts->AtlasWidth = StringUtils::ParseInt(atlasSizeUIValues[widthIdx]);
                m_HasPropertyChanged = true;
            }

            uint32_t heightIdx = findSizeIdx(opts->AtlasHeight);
            if (UI::PropertyDropdown("Atlas Height", atlasSizeUIValues, heightIdx))
            {
                opts->AtlasHeight = StringUtils::ParseInt(atlasSizeUIValues[heightIdx]);
                m_HasPropertyChanged = true;
            }
        }
        m_HasPropertyChanged |= UI::Property("MSDF Pixel Range", opts->AtlasPixelRange, 0.1f, FontImportOptions::MIN_ATLAS_PIXEL_RANGE,
                                             FontImportOptions::MAX_ATLAS_PIXEL_RANGE);
        m_HasPropertyChanged |= UI::PropertyDropdown(
          "Charset Range",
          { "ASCII", "Extended ASCII", "Lower ASCII", "Upper ASCII", "Numbers and Symbols", "Symbol Range", "Decimal Range", "Hex Range" },
          opts->Range);
        if (opts->Range == CharsetRange::DecimalRange || opts->Range == CharsetRange::HexRange || opts->Range == CharsetRange::SymbolRange)
            m_HasPropertyChanged |= UI::PropertyMultiline("Symbols", opts->CustomCharset);
        m_HasPropertyChanged |= UI::Property("Padding", opts->Padding, 0U, 256U);
        m_HasPropertyChanged |= UI::Property("Get Kerning Data", opts->GetKerningData);
        m_HasPropertyChanged |= UI::Property("Tab Width", opts->TabMultiple, 1U, 64U);

        const Ref<AssetMetadata> inspectedMetadata = ProjectLibrary::Get().FindAssetMetadata(m_InspectedAssetPath);
        const UUID inspectedFont = inspectedMetadata != nullptr ? inspectedMetadata->Uuid : UUID::EMPTY;
        if (!inspectedFont.Empty())
        {
            const auto fallbackEnd = std::remove(opts->FallbackFonts.begin(), opts->FallbackFonts.end(), inspectedFont);
            if (fallbackEnd != opts->FallbackFonts.end())
            {
                opts->FallbackFonts.erase(fallbackEnd, opts->FallbackFonts.end());
                m_HasPropertyChanged = true;
            }
        }
        AssetManager* assetManager = AssetManager::TryGet();
        for (size_t index = 0; index < opts->FallbackFonts.size();)
        {
            AssetHandle<Font> fallback;
            if (assetManager != nullptr)
                fallback = static_asset_cast<Font>(assetManager->GetAssetHandle(opts->FallbackFonts[index]));

            if (UIUtils::AssetReference<Font>("Fallback " + std::to_string(index + 1), fallback))
            {
                const UUID selectedFont = fallback.GetUUID();
                if (selectedFont.Empty())
                {
                    opts->FallbackFonts.erase(opts->FallbackFonts.begin() + index);
                    m_HasPropertyChanged = true;
                    continue;
                }

                const bool isDuplicate =
                  std::find(opts->FallbackFonts.begin(), opts->FallbackFonts.end(), selectedFont) != opts->FallbackFonts.end() &&
                  selectedFont != opts->FallbackFonts[index];
                if (selectedFont != inspectedFont && !isDuplicate)
                {
                    opts->FallbackFonts[index] = selectedFont;
                    m_HasPropertyChanged = true;
                }
            }
            ++index;
        }

        if (opts->FallbackFonts.size() < Font::MAX_FALLBACK_FONTS)
        {
            AssetHandle<Font> fallback;
            if (UIUtils::AssetReference<Font>("Add Fallback", fallback) && fallback.HasUUID())
            {
                const UUID selectedFont = fallback.GetUUID();
                const bool isDuplicate = std::find(opts->FallbackFonts.begin(), opts->FallbackFonts.end(), selectedFont) != opts->FallbackFonts.end();
                if (selectedFont != inspectedFont && !isDuplicate)
                {
                    opts->FallbackFonts.push_back(selectedFont);
                    m_HasPropertyChanged = true;
                }
            }
        }

        EndImportInspector(0, ImGui::GetColumnWidth());

        // Make sure the font is imported
        ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions);
    }

    void InspectorPanel::RenderScriptImportInspector()
    {
        // The import options aren't really used here. Only the cache is accessed. Perhaps storing the cache in the
        // import options could be the proper way to do it.
        auto iterFind = m_CachedScriptText.find(m_InspectedAssetPath); // This list should refresh when the asset browser refreshes.
                                                                       // Or we should store this cache there.
        if (iterFind == m_CachedScriptText.end())
        {
            AssetHandle<ScriptCode> scriptCode = static_asset_cast<ScriptCode>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
            // CW_ENGINE_INFO(scriptCode->GetSource());
            m_CachedScriptText[m_InspectedAssetPath] = scriptCode->GetSource();
        }
        // TODO: This should not be the whole text
        ImGui::Text("%s", m_CachedScriptText[m_InspectedAssetPath].c_str());
    }

    void InspectorPanel::RenderTextImportInspector()
    {
        /*auto iterFind = m_CachedScriptText.find(m_InspectedAssetPath);
        if (iterFind == m_CachedScriptText.end())
        {
            Ref<TextureImportOptions> scriptCode =
        StaticRefCast<ScriptCode>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
            CW_ENGINE_INFO(scriptCode->GetSource());
            m_CachedScriptText[m_InspectedAssetPath] = scriptCode->GetSource();
        }
        ImGui::Text("%s", m_CachedScriptText[m_InspectedAssetPath].c_str());*/
    }

    void InspectorPanel::RenderTextureImportInspector()
    {
        ImGui::TextDisabled("Preview");
        AssetHandle<Texture> texture = static_asset_cast<Texture>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
        if (texture)
        {
            Ref<Texture> texturePtr = texture.GetInternalPtr();
            const float sourceWidth = static_cast<float>(texturePtr->GetWidth());
            const float sourceHeight = static_cast<float>(texturePtr->GetHeight());
            const float previewWidth = std::max(1.0f, std::min(ImGui::GetContentRegionAvail().x, 220.0f));
            const float previewHeight =
              std::max(1.0f, sourceWidth > 0.0f ? std::min(180.0f, previewWidth * sourceHeight / sourceWidth) : previewWidth);
            const float fittedWidth = std::max(1.0f, sourceHeight > 0.0f ? previewHeight * sourceWidth / sourceHeight : previewWidth);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - fittedWidth) * 0.5f));
            ImGui::Image(ImGuiVulkanTexture::Get(texturePtr), ImVec2(fittedWidth, previewHeight), { 0.0f, 1.0f }, { 1.0f, 0.0f });
            ImGui::TextDisabled("%u x %u", texturePtr->GetWidth(), texturePtr->GetHeight());
        }
        else
        {
            ImGui::TextDisabled("Preview unavailable");
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::TextDisabled("Import");
        auto* opts = BeginImportInspector<TextureImportOptions>();

        ImGui::TextDisabled("Presets");
        const auto applyPreset = [&](TextureMipMode mode, bool sRGB, TextureDiskFormat diskFormat, bool generateMips) {
            opts->AutomaticFormat = true;
            opts->Shape = TextureShape::TEXTURE_2D;
            opts->MipMode = mode;
            opts->SRGB = sRGB;
            opts->DiskFormat = diskFormat;
            opts->GenerateMips = generateMips;
            m_HasPropertyChanged = true;
        };
        if (ImGui::SmallButton("Color"))
            applyPreset(TextureMipMode::Color, true, TextureDiskFormat::ETC1S, true);
        ImGui::SameLine();
        if (ImGui::SmallButton("Normal map"))
            applyPreset(TextureMipMode::NormalMap, false, TextureDiskFormat::UASTC, true);
        ImGui::SameLine();
        if (ImGui::SmallButton("Data"))
            applyPreset(TextureMipMode::Data, false, TextureDiskFormat::UASTC, true);
        ImGui::SameLine();
        if (ImGui::SmallButton("HDR"))
            applyPreset(TextureMipMode::Color, false, TextureDiskFormat::None, true);
        ImGui::SameLine();
        if (ImGui::SmallButton("UI"))
            applyPreset(TextureMipMode::Color, true, TextureDiskFormat::UASTC, false);

        m_HasPropertyChanged |= UI::Property("Detect format", opts->AutomaticFormat);
        {
            static constexpr std::array<TextureFormat, 15> formats = {
                TextureFormat::R8,      TextureFormat::RG8,  TextureFormat::RGB8,  TextureFormat::RGBA8,  TextureFormat::BGRA8,
                TextureFormat::R16,     TextureFormat::RG16, TextureFormat::RGB16, TextureFormat::RGBA16, TextureFormat::RG16F,
                TextureFormat::RGBA16F, TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGB32F, TextureFormat::RGBA32F,
            };
            static const Vector<String> formatNames = { "R8",     "RG8",   "RGB8",    "RGBA8", "BGRA8", "R16",    "RG16",   "RGB16",
                                                        "RGBA16", "RG16F", "RGBA16F", "R32F",  "RG32F", "RGB32F", "RGBA32F" };
            uint32_t formatIndex = 0;
            const auto selected = std::find(formats.begin(), formats.end(), opts->Format);
            if (selected != formats.end())
                formatIndex = static_cast<uint32_t>(std::distance(formats.begin(), selected));
            UI::ScopedDisable disabled(opts->AutomaticFormat);
            if (UI::PropertyDropdown("Texture format", formatNames, formatIndex))
            {
                opts->Format = formats[formatIndex];
                m_HasPropertyChanged = true;
            }
        }
        m_HasPropertyChanged |= UI::Property("Generate mipmaps", opts->GenerateMips);
        m_HasPropertyChanged |= UI::Property("Generate Environment Map", opts->GenerateEnvironmentMap);
        {
            UI::ScopedDisable disabled(!opts->GenerateMips);
            m_HasPropertyChanged |= UI::Property("Max mip level", opts->MaxMip);
            m_HasPropertyChanged |= UI::PropertyDropdown("Mip filter", { "Box", "Triangle", "Mitchell", "Lanczos 4", "Kaiser" }, opts->MipFilter);
            m_HasPropertyChanged |= UI::PropertyDropdown("Mip content", { "Color", "Normal map", "Data" }, opts->MipMode);
            m_HasPropertyChanged |= UI::Property("Wrap while filtering", opts->MipWrap);
            m_HasPropertyChanged |= UI::Property("Preserve alpha coverage", opts->PreserveAlphaCoverage);
            {
                UI::ScopedDisable coverageDisabled(!opts->PreserveAlphaCoverage);
                m_HasPropertyChanged |= UI::Property("Alpha cutoff", opts->AlphaCutoff, 0.01f, 0.0f, 1.0f);
            }
        }
        m_HasPropertyChanged |= UI::Property("Keep CPU copy", opts->CpuCached);
        m_HasPropertyChanged |= UI::Property("sRGB color space", opts->SRGB);
        m_HasPropertyChanged |= UI::PropertyDropdown("Compression", { "None", "ETC1S (smaller)", "UASTC (higher quality)" }, opts->DiskFormat);
        if (opts->DiskFormat == TextureDiskFormat::UASTC)
            m_HasPropertyChanged |=
              UI::PropertyDropdown("Compression effort", { "Fastest", "Fast", "Balanced", "Thorough", "Maximum" }, opts->UASTCEffort);

        EndImportInspector(0, ImGui::GetColumnWidth());
    }

    void InspectorPanel::RenderShaderImportInspector()
    {
        Ref<ShaderImportOptions> shaderImport = StaticRefCast<ShaderImportOptions>(m_ImportOptions);
        UnorderedMap<String, String>& defines = shaderImport->GetDefines();
        String removeKey;
        String renameFrom;
        String renameTo;
        bool removeRequested = false;
        bool renameRequested = false;
        bool invalidRename = false;
        Vector<String> defineNames;
        defineNames.reserve(defines.size());
        for (CW_MAYBE_UNUSED const auto& [name, value] : defines)
            defineNames.push_back(name);
        std::sort(defineNames.begin(), defineNames.end());

        ImGui::TextDisabled("Shader defines");
        ImGui::TextWrapped("Define compile-time names and optional values. Press Enter to rename a define.");
        ImGui::Dummy(ImVec2(0.0f, 3.0f));

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable("##ShaderDefines", 3, tableFlags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.1f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const String& defineName : defineNames)
            {
                auto define = defines.find(defineName);
                if (define == defines.end())
                    continue;
                const String& key = define->first;
                String& value = define->second;
                ImGui::PushID(key.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                String editedKey = key;
                if (ImGui::InputText("##Name", &editedKey, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue) &&
                    editedKey != key)
                {
                    if (!editedKey.empty() && defines.find(editedKey) == defines.end())
                    {
                        renameFrom = key;
                        renameTo = editedKey;
                        renameRequested = true;
                    }
                    else
                    {
                        invalidRename = true;
                    }
                }

                ImGui::TableNextColumn();
                m_HasPropertyChanged |= ImGui::InputText("##Value", &value, ImGuiInputTextFlags_AutoSelectAll);

                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Remove"))
                {
                    removeKey = key;
                    removeRequested = true;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (renameRequested)
        {
            const auto source = defines.find(renameFrom);
            if (source != defines.end())
            {
                String value = source->second;
                defines.erase(source);
                defines[renameTo] = value;
                m_HasPropertyChanged = true;
            }
        }
        if (removeRequested)
        {
            defines.erase(removeKey);
            m_HasPropertyChanged = true;
        }

        if (invalidRename)
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.32f, 1.0f), "Define names must be unique and cannot be empty.");
        if (defines.empty())
            ImGui::TextDisabled("No shader defines.");

        if (ImGui::Button("Add define"))
        {
            String newName = "NEW_DEFINE";
            uint32_t suffix = 2;
            while (defines.find(newName) != defines.end())
                newName = "NEW_DEFINE_" + std::to_string(suffix++);
            defines[newName] = "";
            m_HasPropertyChanged = true;
        }

        DrawApplyRevert(0.0f, ImGui::GetContentRegionAvail().x);
    }

    void InspectorPanel::RenderMeshImportInspector()
    {
        auto* opts = BeginImportInspector<MeshImportOptions>();

        m_HasPropertyChanged |= UI::Property("Scale factor", opts->ScaleFactor);
        m_HasPropertyChanged |= UI::PropertyDropdown("Index Format", { "Auto", "16 bit", "32 bit" }, opts->IndexFormat);
        m_HasPropertyChanged |= UI::PropertyDropdown("Normals", { "Import", "Calculate", "None" }, opts->NormalsMode);
        m_HasPropertyChanged |= UI::PropertyDropdown("Tangents", { "Import", "Calculate", "None" }, opts->TangentsMode);
        if (opts->NormalsMode == NormalsImportMode::Calculate)
        {
            m_HasPropertyChanged |= UI::Property("Smooth Normals", opts->SmoothNormals);
            if (opts->SmoothNormals)
                m_HasPropertyChanged |= UI::Property("Smoothing Angle", opts->SmoothingAngle, 0.1f, 0.0f, 175.0f);
        }
        m_HasPropertyChanged |= UI::Property("CPU Cached", opts->CpuCached);
        m_HasPropertyChanged |= UI::Property("Optimize", opts->Optimize);
        m_HasPropertyChanged |= UI::Property("Compress", opts->Compress);
        m_HasPropertyChanged |= UI::Property("Import Materials", opts->ImportMaterials);
        if (opts->ImportMaterials)
        {
            UI::ScopedPropertyTooltip tooltip("Cook material textures faster, using more disk space and potentially more GPU memory. "
                                              "Disable for smaller color textures and more thorough normal-map compression.");
            m_HasPropertyChanged |= UI::Property("Fast Texture Compression", opts->FastTextureCompression);
        }
        m_HasPropertyChanged |= UI::Property("Generate Prefab", opts->GeneratePrefab);
        if (opts->GeneratePrefab)
        {
            m_HasPropertyChanged |= UI::Property("Import Lights", opts->ImportLights);
            m_HasPropertyChanged |= UI::Property("Import Cameras", opts->ImportCameras);
        }
        m_HasPropertyChanged |= UI::Property("Import Vertex Colors", opts->ImportVertexColors);
        m_HasPropertyChanged |= UI::Property("Import Morph Targets", opts->ImportMorphMeshes);
        m_HasPropertyChanged |= UI::Property("Import Bone Weights", opts->ImportBones);
        m_HasPropertyChanged |= UI::Property("Import Animations", opts->ImportAnimations);
        m_HasPropertyChanged |= UI::Property("Flip UVs", opts->FlipUVs);
        m_HasPropertyChanged |= UI::Property("Flip Winding Order", opts->FlipWindingOrder);
        m_HasPropertyChanged |= UI::Property("Generate Collision", opts->GenerateCollision);
        if (opts->GenerateCollision)
        {
            constexpr uint32_t minConvexPoints = 8u;
            constexpr uint32_t maxConvexPoints = 4096u;
            if (UI::Property("Convex Point Limit", opts->CollisionMaxConvexPoints, minConvexPoints, maxConvexPoints))
            {
                opts->CollisionMaxConvexPoints = std::clamp(opts->CollisionMaxConvexPoints, minConvexPoints, maxConvexPoints);
                m_HasPropertyChanged = true;
            }
        }

        EndImportInspector(0, ImGui::GetColumnWidth());
    }

    void InspectorPanel::RenderPrefabInspector()
    {
        ImGui::TextColored(ImVec4(0.39f, 0.63f, 1.0f, 1.0f), "Prefab Asset");
        ImGui::Separator();
        ImGui::TextDisabled("Drag this prefab into the Hierarchy or Viewport to instantiate it.");
    }

    void InspectorPanel::DrawHeader()
    {
        if (m_InspectorMode == InspectorMode::GameObject)
            return;

        if (m_InspectedAssetPath.empty())
            return;

        auto drawAssetHeader = [&](const char* assetTypeName) {
            const ImGuiTableFlags flags =
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;
            if (ImGui::BeginTable("##AssetInspectorHeader", 2, flags))
            {
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m_InspectedAssetPath.filename().string().c_str());
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", m_InspectedAssetPath.string().c_str());
                ImGui::TextDisabled("%s import settings", assetTypeName);

                ImGui::TableNextColumn();
                if (ImGui::Button("Reset"))
                {
                    m_ImportOptions = Importer::Get().CreateImportOptions(m_InspectedAssetPath);
                    m_HasPropertyChanged = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Reset import settings");
                ImGui::SameLine();
                if (ImGui::Button("Open"))
                    PlatformUtils::OpenExternally(m_InspectedAssetPath);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Open externally");
                ImGui::EndTable();
            }
            ImGui::Separator();
        };

        switch (m_InspectorMode)
        {
        case InspectorMode::AudioClipImport:
            drawAssetHeader("Audio clip");
            break;
        case InspectorMode::TextureImport:
            drawAssetHeader("Texture");
            break;
        case InspectorMode::FontImport:
            drawAssetHeader("Font");
            break;
        case InspectorMode::PhysicsMaterial:
            drawAssetHeader("Physics Material");
            break;
        case InspectorMode::ScriptImport:
            drawAssetHeader("C# Script");
            break;
        case InspectorMode::ShaderImport:
            drawAssetHeader("Shader");
            break;
        case InspectorMode::MeshImport:
            drawAssetHeader("Mesh");
            break;
        case InspectorMode::Material:
            drawAssetHeader("Material");
            break;
        default: {
            const ImGuiTableFlags flags =
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;
            if (ImGui::BeginTable("##AssetInspectorHeader", 2, flags))
            {
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m_InspectedAssetPath.filename().string().c_str());
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", m_InspectedAssetPath.string().c_str());
                ImGui::TextDisabled("Asset");

                ImGui::TableNextColumn();
                if (ImGui::Button("Open"))
                    PlatformUtils::OpenExternally(m_InspectedAssetPath);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Open externally");
                ImGui::EndTable();
            }
            ImGui::Separator();
            break;
        }
        }
    }

    void InspectorPanel::EndImportInspector(float xOffset, float width)
    {
        UI::EndPropertyGrid();
        DrawApplyRevert(xOffset, width);
    }

    void InspectorPanel::DrawApplyRevert(float xOffset, CW_MAYBE_UNUSED float width)
    {
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        ImGui::SetCursorPosX(xOffset);
        const bool changed = m_HasPropertyChanged;
        ImGui::TextDisabled(changed ? "Import settings have unapplied changes." : "Import settings are up to date.");

        if (!changed)
            ImGui::BeginDisabled();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float buttonWidth = std::max(1.0f, (availableWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
        if (ImGui::Button("Apply", ImVec2(buttonWidth, 0.0f)))
        {
            ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions, true);
            m_HasPropertyChanged = false;
            m_OldImportOptions = m_ImportOptions->Clone();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert", ImVec2(buttonWidth, 0.0f)))
        {
            if (m_OldImportOptions)
                m_ImportOptions = m_OldImportOptions->Clone();
            m_HasPropertyChanged = false;
        }
        if (!changed)
            ImGui::EndDisabled();
    }

    void InspectorPanel::SetSelectedAssetPath(const Path& filepath)
    {
        const bool selectionChanged = m_InspectedAssetPath != filepath;
        if (selectionChanged)
        {
            ResetMaterialPreview();
            m_MaterialParameterSearch.clear();
            ResetAssetUndoTransactions(true);
            FlushPendingAssetSaves();
            m_MaterialSchemaCache.Reset();
        }

        if (filepath.empty())
        {
            if (m_InspectorMode != InspectorMode::Default)
            {
                if (!selectionChanged)
                {
                    ResetAssetUndoTransactions(true);
                    FlushPendingAssetSaves();
                }
                m_EntityInspector.ResetUndoTransactions(true);
                m_MaterialSchemaCache.Reset();
            }
            m_InspectorMode = InspectorMode::Default;
            m_HasPropertyChanged = false;
            m_InspectedAssetPath.clear();
            return;
        }

        m_InspectedAssetPath = filepath;
        m_TemporaryImGuiString.clear();
        if (fs::is_directory(filepath))
        {
            m_InspectorMode = InspectorMode::Default;
            return;
        }

        if (filepath.extension() == ".cwprefab")
        {
            m_InspectorMode = InspectorMode::Prefab;
            return;
        }

        const Ref<LibraryEntry> selectedEntry = ProjectLibrary::Get().FindEntry(filepath);
        if (selectedEntry && selectedEntry->Type == LibraryEntryType::File)
        {
            FileEntry* fileEntry = static_cast<FileEntry*>(selectedEntry.get());
            if (fileEntry->Metadata &&
                (fileEntry->Metadata->Type == AssetType::PhysicsMaterial2D || fileEntry->Metadata->Type == AssetType::PhysicsMaterial))
            {
                m_InspectorMode = InspectorMode::PhysicsMaterial;
                m_HasPropertyChanged = false;
                m_ImportOptions = fileEntry->Metadata->ImportOptions;
                m_OldImportOptions = m_ImportOptions ? m_ImportOptions->Clone() : nullptr;
                return;
            }
        }

        SpecificImporter* const importer = Importer::Get().GetImporterForFile(filepath);
        if (importer != nullptr)
        {
            if (dynamic_cast<AudioClipImporter*>(importer))
                m_InspectorMode = InspectorMode::AudioClipImport;
            else if (dynamic_cast<TextureImporter*>(importer))
                m_InspectorMode = InspectorMode::TextureImport;
            else if (dynamic_cast<ScriptImporter*>(importer))
                m_InspectorMode = InspectorMode::ScriptImport;
            else if (dynamic_cast<TextFileImporter*>(importer))
                m_InspectorMode = InspectorMode::TextImport;
            else if (dynamic_cast<ShaderImporter*>(importer))
                m_InspectorMode = InspectorMode::ShaderImport;
            else if (dynamic_cast<FontImporter*>(importer))
                m_InspectorMode = InspectorMode::FontImport;
            else if (dynamic_cast<MaterialImporter*>(importer))
                m_InspectorMode = InspectorMode::Material;
            else if (dynamic_cast<MeshImporter*>(importer))
                m_InspectorMode = InspectorMode::MeshImport;
            else
                m_InspectorMode = InspectorMode::Default;
        }
        else
        {
            m_InspectorMode = InspectorMode::Default;
        }

        const Ref<LibraryEntry> entry = ProjectLibrary::Get().FindEntry(filepath);
        if (entry != nullptr)
        {
            if (entry->Type == LibraryEntryType::File)
            {
                FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
                if (fileEntry->Metadata != nullptr)
                {
                    m_HasPropertyChanged = false;
                    m_ImportOptions = fileEntry->Metadata->ImportOptions;
                    m_OldImportOptions = m_ImportOptions->Clone();
                }
            }
        }
    }

    void InspectorPanel::SetSelectedEntity(Entity e) { SetSelectedEntities(e, e ? Vector<Entity>{ e } : Vector<Entity>{}); }

    void InspectorPanel::SetSelectedEntities(Entity primary, const Vector<Entity>& entities)
    {
        const bool sameScene = m_InspectedEntity && primary && m_InspectedEntity.GetScene() == primary.GetScene();
        const bool sameSelection = m_InspectedEntity == primary && m_InspectedEntities == entities;
        if (m_InspectorMode != InspectorMode::GameObject || !sameSelection)
        {
            ResetMaterialPreview();
            ResetAssetUndoTransactions(true);
            FlushPendingAssetSaves();
        }
        if (!sameSelection)
            m_EntityInspector.ResetUndoTransactions(sameScene);
        m_MaterialSchemaCache.Reset();
        m_InspectorMode = InspectorMode::GameObject;
        m_InspectedEntity = primary;
        m_InspectedEntities = entities;
        m_HasPropertyChanged = false;
    }

    void InspectorPanel::SetInspectorMode(InspectorMode mode)
    {
        if (m_InspectorMode != mode)
        {
            ResetMaterialPreview();
            ResetAssetUndoTransactions(true);
            FlushPendingAssetSaves();
            m_EntityInspector.ResetUndoTransactions(true);
            m_MaterialSchemaCache.Reset();
        }
        m_InspectorMode = mode;
        m_HasPropertyChanged = false;
    }

    void InspectorPanel::ResetUndoTransactions(bool finishInteraction)
    {
        ResetAssetUndoTransactions(finishInteraction);
        m_EntityInspector.ResetUndoTransactions(finishInteraction);
    }

} // namespace Crowny
