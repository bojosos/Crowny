#include "cwepch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Scene/SceneManager.h"

#include "Panels/ComponentEditor.h"
#include "Panels/InspectorPanel.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "UI/Properties.h"
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
        m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");

        // Rendering
        m_ComponentEditor.PushComponentGroup("Rendering");
        m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
        m_ComponentEditor.RegisterComponent<LightComponent>("Light");
        m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
        m_ComponentEditor.RegisterComponent<AnimationComponent>("Animation");
        m_ComponentEditor.RegisterComponent<TextComponent>("Text");
        m_ComponentEditor.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
        m_ComponentEditor.RegisterComponent<ProceduralMeshComponent>("Procedural Mesh", [this](Entity entity) {
            auto& comp = entity.GetComponent<ProceduralMeshComponent>();

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
        m_ComponentEditor.PopComponentGroup();

        // Physics
        m_ComponentEditor.PushComponentGroup("Physics");
        m_ComponentEditor.RegisterComponent<Rigidbody2DComponent>("Rigidbody 2D");
        m_ComponentEditor.RegisterComponent<BoxCollider2DComponent>("Box Collider 2D");
        m_ComponentEditor.RegisterComponent<CircleCollider2DComponent>("Circle Collider 2D");
        m_ComponentEditor.RegisterComponent<Rigidbody3DComponent>("Rigidbody 3D");
        m_ComponentEditor.RegisterComponent<BoxCollider3DComponent>("Box Collider 3D");
        m_ComponentEditor.RegisterComponent<SphereCollider3DComponent>("Sphere Collider 3D");
        m_ComponentEditor.RegisterComponent<CapsuleCollider3DComponent>("Capsule Collider 3D");
        m_ComponentEditor.PopComponentGroup();

        // Audio
        m_ComponentEditor.PushComponentGroup("Audio");
        m_ComponentEditor.RegisterComponent<AudioListenerComponent>("Audio Listener");
        m_ComponentEditor.RegisterComponent<AudioSourceComponent>("Audio Source");
        m_ComponentEditor.PopComponentGroup();

        // Scripting
        m_ComponentEditor.RegisterComponent<ManagedScriptComponent>("C# Script");
    }

    InspectorPanel::~InspectorPanel()
    {
        ResetPhysicsMaterialUndoTransaction(true);
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
                    case AssetType::Font:
                        const AssetHandle<Font> font = static_asset_cast<Font>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change =
                          AddComponentToSelection<TextComponent>(selectedEntities, [&](Entity, TextComponent& component) { component.Font = font; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    case AssetType::Mesh:
                        const AssetHandle<Mesh> mesh = static_asset_cast<Mesh>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change = AddComponentToSelection<MeshRendererComponent>(
                          selectedEntities, [&](Entity, MeshRendererComponent& component) { component.MeshHandle = mesh; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
                    case AssetType::Texture:
                        const AssetHandle<Texture> texture = static_asset_cast<Texture>(ProjectLibrary::Get().Load(fileEntry));
                        const SelectionComponentChange change = AddComponentToSelection<SpriteRendererComponent>(
                          selectedEntities, [&](Entity, SpriteRendererComponent& component) { component.Texture = texture; });
                        UndoRedo::Get().RegisterAction(change.Action);
                        break;
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
            m_ComponentEditor.Render(m_InspectedEntity, m_InspectedEntities);
            break;
        case InspectorMode::Material:
            if (m_ImportOptions)
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

    void InspectorPanel::RenderMaterialInspector()
    {
        AssetHandle<Material> mat = AssetManager::TryGet()->Load<Material>(m_InspectedAssetPath);
        if (!mat)
            return;

        const Ref<Asset> inspectedAsset = StaticRefCast<Asset>(mat.GetInternalPtr());
        const auto applyEdit = [this, &inspectedAsset](bool changed, auto&& apply) {
            if (changed)
                apply();
            ObserveAssetEdit(inspectedAsset, changed);
        };

        UI::BeginPropertyGrid();

        // Shader selector — allows changing the shader (like Unity's material type)
        AssetHandle<Shader> currentShader = mat->GetShader();
        applyEdit(UIUtils::AssetSearch<Shader>("Shader", currentShader), [&]() { mat->SetShader(currentShader); });

        int32_t alphaMode = mat->HasAlphaModeOverride() ? static_cast<int32_t>(mat->GetAlphaMode()) + 1 : 0;
        const bool alphaModeChanged =
          UI::PropertyDropdown("Alpha Mode", { "Inferred (Shader)", "Opaque", "Alpha Mask", "Premultiplied", "Additive", "Weighted OIT" }, alphaMode);
        applyEdit(alphaModeChanged, [&]() {
            if (alphaMode == 0)
                mat->ClearAlphaModeOverride();
            else
                mat->SetAlphaMode(static_cast<AlphaMode>(alphaMode - 1));
        });

        if (MaterialRenderClassifier::Classify(*mat).Model == MaterialModel::Toon)
        {
            UI::Pre("Apply Preset");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonWidth = std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f);
            ImGui::PushID("ToonMaterialPreset");
            if (ImGui::Button("Classic", ImVec2(buttonWidth, 0.0f)))
                mat->ApplyToonPreset(ToonMaterialPreset::Classic);
            ImGui::SameLine();
            if (ImGui::Button("Soft", ImVec2(buttonWidth, 0.0f)))
                mat->ApplyToonPreset(ToonMaterialPreset::Soft);
            ImGui::SameLine();
            if (ImGui::Button("Hatched", ImVec2(buttonWidth, 0.0f)))
                mat->ApplyToonPreset(ToonMaterialPreset::Hatched);
            ImGui::PopID();
            UI::Post();
        }

        ImGui::Separator();

        const Vector<ShaderParameterDesc>& params = m_MaterialSchemaCache.Resolve(*mat);

        for (const auto& param : params)
        {
            switch (param.Type)
            {
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
                applyEdit(modified, [&]() { mat->SetTexture(param.Identifier, texHandle); });
                break;
            }
            default:
                break;
            }
        }

        UI::EndPropertyGrid();
    }

    void InspectorPanel::RenderPhysicsMaterialInspector()
    {
        AssetHandle<Asset> asset = ProjectLibrary::Get().Load(m_InspectedAssetPath);
        if (!asset)
        {
            ResetPhysicsMaterialUndoTransaction(false);
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

    void InspectorPanel::ResetPhysicsMaterialUndoTransaction(bool finishInteraction)
    {
        if (UndoRedo* undoRedo = UndoRedo::TryGet())
        {
            if (finishInteraction)
                undoRedo->FinishComponentScope(m_PhysicsMaterialUndo);
            else
                undoRedo->CancelComponentScope(m_PhysicsMaterialUndo);
        }
        m_PhysicsMaterialUndo->Reset();
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
        for (const auto& [name, value] : defines)
        {
            (void)value;
            defineNames.push_back(name);
        }
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
        m_HasPropertyChanged |= UI::Property("Import Vertex Colors", opts->ImportVertexColors);
        m_HasPropertyChanged |= UI::Property("Import Morph Targets", opts->ImportMorphMeshes);
        m_HasPropertyChanged |= UI::Property("Import Bone Weights", opts->ImportBones);
        m_HasPropertyChanged |= UI::Property("Import Animations", opts->ImportAnimations);
        m_HasPropertyChanged |= UI::Property("Flip UVs", opts->FlipUVs);
        m_HasPropertyChanged |= UI::Property("Flip Winding Order", opts->FlipWindingOrder);

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

    void InspectorPanel::DrawApplyRevert(float xOffset, float width)
    {
        (void)width;
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
            ResetPhysicsMaterialUndoTransaction(true);
            m_ComponentEditor.ResetUndoTransactions(true);
            FlushPendingAssetSaves();
            m_MaterialSchemaCache.Reset();
        }

        if (filepath.empty())
        {
            if (m_InspectorMode != InspectorMode::Default)
            {
                if (!selectionChanged)
                {
                    ResetPhysicsMaterialUndoTransaction(true);
                    m_ComponentEditor.ResetUndoTransactions(true);
                    FlushPendingAssetSaves();
                }
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
            ResetPhysicsMaterialUndoTransaction(true);
            FlushPendingAssetSaves();
        }
        if (!sameSelection)
            m_ComponentEditor.ResetUndoTransactions(sameScene);
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
            ResetPhysicsMaterialUndoTransaction(true);
            FlushPendingAssetSaves();
            m_ComponentEditor.ResetUndoTransactions(true);
            m_MaterialSchemaCache.Reset();
        }
        m_InspectorMode = mode;
        m_HasPropertyChanged = false;
    }

    void InspectorPanel::ResetUndoTransactions(bool finishInteraction)
    {
        ResetPhysicsMaterialUndoTransaction(finishInteraction);
        m_ComponentEditor.ResetUndoTransactions(finishInteraction);
    }

} // namespace Crowny
