#include "cwepch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Serialization/MaterialSerializer.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Scene/SceneManager.h"

#include "Panels/ComponentEditor.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"

#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Import/AudioClipImporter.h"
#include "Crowny/Import/FontImporter.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Import/ScriptImporter.h"
#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Import/TextFileImporter.h"
#include "Crowny/Import/TextureImporter.h"

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
        m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
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
                    graph->AddNode(NodeRegistry::Get().Create("GeometryOutputNode"));

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
                            comp.Graph = static_asset_cast<NodeGraphAsset>(gAssetManager->LoadFromUUID(fileEntry->Metadata->Uuid));
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

                            String label = String(input.Name.c_str()) + "##" + input.ID.ToString();

                            switch (input.DataType)
                            {
                            case PinDataType::Float: {
                                float v = std::holds_alternative<float>(val)
                                            ? std::get<float>(val)
                                            : (std::holds_alternative<float>(input.DefaultValue) ? std::get<float>(input.DefaultValue) : 0.0f);
                                if (ImGui::DragFloat(label.c_str(), &v, 0.01f))
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
                                if (ImGui::DragInt(label.c_str(), &v))
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
                                if (ImGui::DragFloat3(label.c_str(), &v.x, 0.01f))
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
                                if (ImGui::Checkbox(label.c_str(), &v))
                                {
                                    val = v;
                                    comp.NeedsEvaluation = true;
                                }
                                break;
                            }
                            default:
                                break;
                            }
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
        m_ComponentEditor.PopComponentGroup();

        // Audio
        m_ComponentEditor.PushComponentGroup("Audio");
        m_ComponentEditor.RegisterComponent<AudioListenerComponent>("Audio Listener");
        m_ComponentEditor.RegisterComponent<AudioSourceComponent>("Audio Source");
        m_ComponentEditor.PopComponentGroup();

        // Scripting
        m_ComponentEditor.RegisterComponent<MonoScriptComponent>("C# Script");
    }

    void InspectorPanel::HandleInspectorDragDrop(Entity selectedEntity)
    {
        if (ImGui::BeginDragDropTarget()) // Add components when files are dropped on entities in the inspector (C#
                                          // script, AudioSource, etc...)
        {
            auto hasComponentCallback = [selectedEntity](const FileEntry* entry) {
                switch (entry->Metadata->Type)
                {
                case AssetType::AudioClip:
                    return !selectedEntity.HasComponent<AudioSourceComponent>();
                case AssetType::Mesh:
                    return !selectedEntity.HasComponent<MeshRendererComponent>();
                case AssetType::ScriptCode:
                    return true; // You can always add more scripts!!!
                case AssetType::Texture:
                    return !selectedEntity.HasComponent<SpriteRendererComponent>();
                case AssetType::Font:
                    return !selectedEntity.HasComponent<TextComponent>();
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
                        if (!selectedEntity.HasComponent<AudioSourceComponent>())
                        {
                            AudioSourceComponent& audioSource = selectedEntity.AddComponent<AudioSourceComponent>();
                            AssetHandle<AudioClip> clip = static_asset_cast<AudioClip>(ProjectLibrary::Get().Load(fileEntry));
                            audioSource.SetClip(clip);
                        }
                        break;
                    }
                    case AssetType::ScriptCode: {
                        const String className = fileEntry->Filepath.filename().replace_extension("").string();
                        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
                        bool exists = false;
                        if (selectedEntity.HasComponent<MonoScriptComponent>())
                        {
                            auto& scripts = selectedEntity.GetComponent<MonoScriptComponent>().Scripts;
                            for (const auto& script : scripts)
                            {
                                if (script.GetTypeName() == className)
                                    exists = true;
                            }
                        }
                        // TODO: Parse the namespace and class name
                        if (!exists)
                            activeScene->AddScriptComponent(selectedEntity, "Sandbox", className);
                        break;
                    }
                    case AssetType::Font:
                        if (!selectedEntity.HasComponent<TextComponent>())
                        {
                            TextComponent& textComponent = selectedEntity.AddComponent<TextComponent>();
                            AssetHandle<Font> font = static_asset_cast<Font>(ProjectLibrary::Get().Load(fileEntry));
                            textComponent.Font = font;
                        }
                        break;
                    case AssetType::Mesh:
                        if (!selectedEntity.HasComponent<MeshRendererComponent>())
                        {
                            MeshRendererComponent& meshComponent = selectedEntity.AddComponent<MeshRendererComponent>();
                            AssetHandle<Mesh> mesh = static_asset_cast<Mesh>(ProjectLibrary::Get().Load(fileEntry));
                            meshComponent.MeshHandle = mesh;
                        }
                        break;
                    case AssetType::Texture:
                        if (!selectedEntity.HasComponent<SpriteRendererComponent>())
                        {
                            SpriteRendererComponent& spriteComponent = selectedEntity.AddComponent<SpriteRendererComponent>();
                            AssetHandle<Texture> texture = static_asset_cast<Texture>(ProjectLibrary::Get().Load(fileEntry));
                            spriteComponent.Texture = texture;
                        }
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void InspectorPanel::Render()
    {
        BeginPanel();
        if (!IsShown())
        {
            EndPanel();
            return;
        }

        auto activeScene = gSceneManager->GetActiveScene();
        if (!activeScene)
        {
            EndPanel();
            return;
        }

        ImGui::BeginChild("InspectorChild");
        DrawHeader();

        switch (m_InspectorMode)
        {
        case InspectorMode::GameObject:
            m_ComponentEditor.Render();
            break;
        case InspectorMode::Material:
            if (m_ImportOptions)
                RenderMaterialInspector();
            break;
        case InspectorMode::PhysicsMaterial:
            if (m_ImportOptions)
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
            break;
        }

        ImGui::EndChild();
        Entity selectedEntity = HierarchyPanel::GetSelectedEntity();
        if (m_InspectorMode == InspectorMode::GameObject && selectedEntity)
            HandleInspectorDragDrop(selectedEntity);
        EndPanel();
    }

    static String AutoDisplayName(const String& identifier)
    {
        // Convert camelCase/snake_case to "Title Case": "roughness" -> "Roughness", "albedoMap" -> "Albedo Map"
        String result;
        for (size_t i = 0; i < identifier.size(); i++)
        {
            char c = identifier[i];
            if (i == 0)
            {
                result += (char)std::toupper(c);
            }
            else if (std::isupper(c))
            {
                result += ' ';
                result += c;
            }
            else if (c == '_')
            {
                result += ' ';
            }
            else
            {
                result += c;
            }
        }
        return result;
    }

    static ShaderParamType MapDataType(ShaderDataType dt, bool isColor)
    {
        switch (dt)
        {
        case ShaderDataType::Float: return ShaderParamType::Float;
        case ShaderDataType::Float2: return ShaderParamType::Float2;
        case ShaderDataType::Float3: return isColor ? ShaderParamType::Color3 : ShaderParamType::Float3;
        case ShaderDataType::Float4: return isColor ? ShaderParamType::Color4 : ShaderParamType::Float4;
        case ShaderDataType::Int: return ShaderParamType::Int;
        case ShaderDataType::Int2: return ShaderParamType::Int2;
        case ShaderDataType::Int3: return ShaderParamType::Int3;
        case ShaderDataType::Int4: return ShaderParamType::Int4;
        case ShaderDataType::Bool: return ShaderParamType::Bool;
        case ShaderDataType::Mat3: return ShaderParamType::Mat3;
        case ShaderDataType::Mat4: return ShaderParamType::Mat4;
        default: return ShaderParamType::Float;
        }
    }

    static ShaderParamType MapResourceType(UniformResourceType rt)
    {
        switch (rt)
        {
        case SAMPLER2D:
        case TEXTURE2D: return ShaderParamType::Texture2D;
        case SAMPLER3D:
        case TEXTURE3D: return ShaderParamType::Texture3D;
        case SAMPLERCUBE:
        case TEXTURECUBE: return ShaderParamType::TextureCube;
        default: return ShaderParamType::Texture2D;
        }
    }

    static Vector<ShaderParameterDesc> BuildInspectorParameters(const Material& mat)
    {
        Vector<ShaderParameterDesc> result;

        // Collect annotations from all shader stages
        UnorderedMap<String, AnnotationSet> allAnnotations;
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const auto& annotations = mat.GetAnnotations((ShaderType)i);
            for (const auto& [name, anno] : annotations)
                allAnnotations[name] = anno;
        }

        // Data parameters from uniform buffer members
        const auto& bindings = mat.GetBindings();
        for (const auto& [name, member] : bindings)
        {
            // Skip internal blocks (prefixed with cw_)
            if (member.BufferName.rfind("cw_", 0) == 0)
                continue;

            ShaderParameterDesc desc;
            desc.Identifier = name;
            desc.BlockName = member.BufferName;
            desc.Offset = member.Offset;

            // Apply annotations if present
            auto annoIt = allAnnotations.find(name);
            bool isColor = false;
            if (annoIt != allAnnotations.end())
            {
                const auto& anno = annoIt->second;
                if (!anno.DisplayName.empty())
                    desc.DisplayName = anno.DisplayName;
                if (anno.IsHidden)
                    continue; // Skip hidden params
                isColor = anno.IsColor;
                if (anno.IsHDR)
                    desc.Flags.Set(ShaderParamFlag::HDR);
                if (anno.HasRange)
                {
                    desc.RangeMin = anno.RangeMin;
                    desc.RangeMax = anno.RangeMax;
                    desc.HasRange = true;
                }
            }

            desc.Type = MapDataType(member.DataType, isColor);

            if (desc.DisplayName.empty())
                desc.DisplayName = AutoDisplayName(name);

            // Sort order: block binding slot * 1000 + member offset (gives declaration order)
            desc.SortOrder = mat.GetBlockBindingSlot(member.BufferName) * 1000 + member.Offset;

            result.push_back(desc);
        }

        // Texture parameters
        const auto textures = mat.GetTextures();
        for (const auto& [name, descInfo] : textures)
        {
            // Skip internal textures (prefixed with cw_)
            if (name.rfind("cw_", 0) == 0)
                continue;

            ShaderParameterDesc desc;
            desc.Identifier = name;
            desc.Type = MapResourceType(descInfo.Type);
            desc.Set = descInfo.Set;
            desc.Slot = descInfo.Slot;

            auto annoIt = allAnnotations.find(name);
            if (annoIt != allAnnotations.end())
            {
                if (annoIt->second.IsHidden)
                    continue;
                if (!annoIt->second.DisplayName.empty())
                    desc.DisplayName = annoIt->second.DisplayName;
            }

            if (desc.DisplayName.empty())
                desc.DisplayName = AutoDisplayName(name);

            // Textures sorted after data params
            desc.SortOrder = 100000 + descInfo.Slot;

            result.push_back(desc);
        }

        // Sort by SortOrder for deterministic, declaration-order display
        std::sort(result.begin(), result.end(),
                  [](const ShaderParameterDesc& a, const ShaderParameterDesc& b) { return a.SortOrder < b.SortOrder; });

        return result;
    }

    void InspectorPanel::RenderMaterialInspector()
    {
        AssetHandle<Material> mat = gAssetManager->Load<Material>(m_InspectedAssetPath);
        if (!mat)
            return;

        UI::BeginPropertyGrid();

        // Shader selector — allows changing the shader (like Unity's material type)
        AssetHandle<Shader> currentShader = mat->GetShader();
        if (UIUtils::AssetSearch<Shader>("Shader", currentShader))
        {
            mat->SetShader(currentShader);
        }

        ImGui::Separator();

        // Build sorted, filtered parameter list
        Vector<ShaderParameterDesc> params = BuildInspectorParameters(*mat.GetInternalPtr());

        for (const auto& param : params)
        {
            switch (param.Type)
            {
            case ShaderParamType::Float:
            {
                float value = mat->GetDataParam<float>(param.Identifier);
                bool modified = param.HasRange
                    ? UI::PropertySlider(param.DisplayName.c_str(), value, param.RangeMin, param.RangeMax)
                    : UI::Property(param.DisplayName.c_str(), value);
                if (modified)
                    mat->SetFloat(param.Identifier, value);
                break;
            }
            case ShaderParamType::Float2:
            {
                glm::vec2 value = mat->GetDataParam<glm::vec2>(param.Identifier);
                if (UI::Property(param.DisplayName.c_str(), value))
                    mat->SetFloat2(param.Identifier, value);
                break;
            }
            case ShaderParamType::Float3:
            {
                glm::vec3 value = mat->GetDataParam<glm::vec3>(param.Identifier);
                if (UI::Property(param.DisplayName.c_str(), value))
                    mat->SetVector3(param.Identifier, value);
                break;
            }
            case ShaderParamType::Float4:
            {
                glm::vec4 value = mat->GetDataParam<glm::vec4>(param.Identifier);
                if (UI::Property(param.DisplayName.c_str(), value))
                    mat->SetColor(param.Identifier, value);
                break;
            }
            case ShaderParamType::Color3:
            {
                // Read as vec3, display with color picker
                glm::vec3 value = mat->GetDataParam<glm::vec3>(param.Identifier);
                ImGuiColorEditFlags flags = param.Flags.IsSet(ShaderParamFlag::HDR) ? ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float : 0;
                if (UI::PropertyColor(param.DisplayName.c_str(), value, flags))
                    mat->SetVector3(param.Identifier, value);
                break;
            }
            case ShaderParamType::Color4:
            {
                // Read as vec4, display with color picker
                glm::vec4 value = mat->GetDataParam<glm::vec4>(param.Identifier);
                ImGuiColorEditFlags flags = param.Flags.IsSet(ShaderParamFlag::HDR) ? ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float : 0;
                if (UI::PropertyColor(param.DisplayName.c_str(), value, flags))
                    mat->SetColor(param.Identifier, value);
                break;
            }
            case ShaderParamType::Int:
            {
                int value = mat->GetDataParam<int>(param.Identifier);
                if (UI::Property(param.DisplayName.c_str(), value))
                    mat->SetInt(param.Identifier, value);
                break;
            }
            case ShaderParamType::Bool:
            {
                bool value = mat->GetDataParam<bool>(param.Identifier);
                if (UI::Property(param.DisplayName.c_str(), value))
                    mat->SetBool(param.Identifier, value);
                break;
            }
            case ShaderParamType::Texture2D:
            case ShaderParamType::Texture3D:
            case ShaderParamType::TextureCube:
            {
                AssetHandle<Texture> texHandle = mat->GetTextureHandle(param.Identifier);
                if (UIUtils::AssetSearch<Texture>(param.DisplayName, texHandle))
                    mat->SetTexture(param.Identifier, texHandle);
                break;
            }
            default:
                break;
            }
        }

        UI::EndPropertyGrid();

        // Auto-save: if params changed, write .cwmat after a short debounce
        using clock = std::chrono::steady_clock;
        uint64_t currentVersion = mat->GetParamVersion();
        auto now = clock::now();
        if (m_MaterialLastSaveVersion != currentVersion && now - m_MaterialLastSaveTime >= std::chrono::seconds(2))
        {
            MaterialSerializer serializer(mat.GetInternalPtr());
            serializer.Serialize(m_InspectedAssetPath);
            m_MaterialLastSaveVersion = currentVersion;
            m_MaterialLastSaveTime = now;
        }
    }

    void InspectorPanel::RenderPhysicsMaterialInspector() {}

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
        float yPos = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
        ImGui::SetCursorPosY(yPos);
        if (ImGui::Button("Play"))
        {
            if (m_HasPropertyChanged) // Why did I do this?
                ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions, true);
            AssetHandle<AudioClip> clip = static_asset_cast<AudioClip>(ProjectLibrary::Get().Load(m_InspectedAssetPath));
            gAudioManager->StopManualSources();
            gAudioManager->Play("Inspector", clip);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            gAudioManager->StopManualSources();
        ImGui::SameLine();
        const float progress = gAudioManager->GetGlobalSourceProgress("Inspector");
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
        if (opts->AutomaticFontSampling)
            m_HasPropertyChanged |= UI::Property("Sampling Size", opts->SamplingFontSize);

        UI::SetTooltip("Static atlases use a predefined charset range. On the other hand Dynamic atlases are populated \
                dynamically during runtime. Static atlases use more memory but are more efficient during execution.");

        dropdownIdx = opts->DynamicFontAtlas ? 1 : 0;
        if (UI::PropertyDropdown("Atlas Mode", { "Static", "Dynamic" }, dropdownIdx))
        {
            m_HasPropertyChanged = true;
            opts->DynamicFontAtlas = dropdownIdx == 1 ? true : false;
        }

        // Only static fonts will need these options.
        if (!opts->DynamicFontAtlas)
        {

            UI::Property("Auto size atlas", opts->AutoSizeAtlas);

            if (opts->AutoSizeAtlas)
            {
                m_HasPropertyChanged |= UI::PropertyDropdown(
                  "Dimension Constraints", { "Power of Two Square", "Power of Two Rectangle", "Multiple of Four Square", "Even Square" },
                  opts->AtlasDimensionsConstraint);
            }
            else
            {
                Vector<String> atlasSizeUIValues = { "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192" };
                auto findSizeIdx = [](uint32_t size) -> uint32_t {
                    uint32_t idx = 0;
                    while (size > 0)
                    {
                        idx++;
                        size /= 2;
                    }
                    return idx - 2;
                };

                uint32_t widthIdx = findSizeIdx(opts->AtlasWidth);
                if (UI::PropertyDropdown("Atlas Width", atlasSizeUIValues, widthIdx))
                {
                    opts->AtlasWidth = (uint32_t)glm::pow(2, widthIdx);
                    m_HasPropertyChanged = true;
                }

                uint32_t heightIdx = findSizeIdx(opts->AtlasHeight);
                if (UI::PropertyDropdown("Atlas Height", atlasSizeUIValues, heightIdx))
                {
                    opts->AtlasHeight = StringUtils::ParseInt(atlasSizeUIValues[heightIdx]);
                    m_HasPropertyChanged = true;
                }
            }
            m_HasPropertyChanged |= UI::PropertyDropdown(
              "Charset Range",
              { "ASCII", "Extended ASCII", "Lower ASCII", "Upper ASCII", "Numbers and Symbols", "Symbol Range", "Decimal Range", "Hex Range" },
              opts->Range);
            if (opts->Range == CharsetRange::DecimalRange || opts->Range == CharsetRange::HexRange || opts->Range == CharsetRange::SymbolRange)
                m_HasPropertyChanged |= UI::PropertyMultiline("Symbols", opts->CustomCharset); // TODO: Replace this with multiline input
        }
        m_HasPropertyChanged |= UI::Property("Padding", opts->Padding);
        m_HasPropertyChanged |= UI::Property("Get Kerning Data", opts->GetKerningData);

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
        auto* opts = BeginImportInspector<TextureImportOptions>();

        m_HasPropertyChanged |= UI::Property("Auto Format", opts->AutomaticFormat);
        m_HasPropertyChanged |= UI::PropertyDropdown("Texture Shape", { "1D", "2D", "3D", "Cubemap" }, opts->Shape);
        // m_HasPropertyChanged |= UI::PropertyDropdown("Texture Format", { "R" }, opts->Format);
        m_HasPropertyChanged |= UI::Property("Generate Mipmaps", opts->GenerateMips);
        m_HasPropertyChanged |= UI::Property("Max Mip Level", opts->MaxMip);
        m_HasPropertyChanged |= UI::Property("CPU Cached", opts->CpuCached);
        m_HasPropertyChanged |= UI::Property("sRGB", opts->SRGB);
        m_HasPropertyChanged |=
          UI::PropertyDropdown("Compression Format", { "None", "ETC1S (lower quality)", "UASTC (higher quality)" }, opts->DiskFormat);

        EndImportInspector(0, ImGui::GetColumnWidth());
    }

    void InspectorPanel::RenderShaderImportInspector()
    {
        Ref<ShaderImportOptions> shaderImport = StaticRefCast<ShaderImportOptions>(m_ImportOptions);
        ImGui::Columns(2);
        ImGui::Text("Defines");
        ImGui::NextColumn();
        ImGui::NextColumn();
        UnorderedMap<String, String>& defines = shaderImport->GetDefines(); // this needs a bit more work, unordered map bad
        uint32_t id = 0;
        for (auto kv : defines)
        {
            ImGui::PushID(id++);
            std::string key = kv.first;
            if (ImGui::InputText("##defineKey", &key, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                defines[key] = kv.second;
            ImGui::NextColumn();
            ImGui::InputText("##defineValue", &kv.second, ImGuiInputTextFlags_AutoSelectAll);
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::NextColumn();
        if (ImGui::Button("+"))
            defines[""] = "";
        ImGui::SameLine();
        if (ImGui::Button("-"))
            defines.erase(std::prev(defines.end()));
        float x = ImGui::GetCursorPosX();
        float width = ImGui::GetColumnWidth();
        // ImGui::NextColumn();
        ImGui::NextColumn();
        ImGui::Columns(1);
        DrawApplyRevert(x, width);
        ImGui::NextColumn();
        ImGui::NextColumn();
        DrawApplyRevert(x, width);
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
        m_HasPropertyChanged |= UI::Property("Optimize", opts->Optimize);
        m_HasPropertyChanged |= UI::Property("Compress", opts->Compress);
        m_HasPropertyChanged |= UI::Property("Keep Quads", opts->KeepQuads);
        // m_HasPropertyChanged |= UI::Property("Enable Read/Write", opts->EnableReadWrite);

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
        // Entity inspection header -- show the entity name
        if (m_InspectorMode == InspectorMode::GameObject)
        {
            if (m_InspectedEntity)
            {
                ImGui::Text("%s", m_InspectedEntity.GetName().c_str());
                ImGui::Separator();
            }
            return;
        }

        if (m_InspectedAssetPath.empty())
            return;

        // Helper: draws the asset import header with Reset and Open buttons
        auto drawAssetHeader = [&](const char* assetTypeName) {
            float maxx = ImGui::GetContentRegionAvail().x;
            ImGui::Text("%s", (m_InspectedAssetPath.filename().string() + " (" + assetTypeName + ") Import Settings").c_str());
            float padding = ImGui::GetStyle().FramePadding.x;
            float open = ImGui::CalcTextSize("Open").x;
            float reset = ImGui::CalcTextSize("Reset").x;
            ImGui::SameLine();
            ImGui::SetCursorPosX(maxx - open - reset - padding * 4);
            if (ImGui::Button("Reset"))
            {
                m_ImportOptions = Importer::Get().CreateImportOptions(m_InspectedAssetPath);
                m_HasPropertyChanged = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Reset the import properties.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
            if (ImGui::Button("Open"))
                PlatformUtils::OpenExternally(m_InspectedAssetPath);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Open in an external program.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::Separator();
        };

        switch (m_InspectorMode)
        {
        case InspectorMode::AudioClipImport:
            drawAssetHeader("Audio Clip");
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
            // Fallback for asset modes without a specific label (Prefab, TextImport, etc.)
            float maxx = ImGui::GetContentRegionAvail().x;
            ImGui::Text("%s", m_InspectedAssetPath.filename().string().c_str());
            float padding = ImGui::GetStyle().FramePadding.x;
            float open = ImGui::CalcTextSize("Open").x;
            ImGui::SameLine();
            ImGui::SetCursorPosX(maxx - open);
            if (ImGui::Button("Open"))
                PlatformUtils::OpenExternally(m_InspectedAssetPath);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Open externally.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
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
        ImGui::Separator();
        float padding = ImGui::GetStyle().FramePadding.x;
        ImGui::SetCursorPosX(xOffset);
        bool changed = m_HasPropertyChanged;
        if (!changed)
            ImGui::BeginDisabled();
        if (ImGui::Button("Apply", ImVec2(width * 0.5f - padding * 4, 0)))
        {
            ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions, true);
            m_HasPropertyChanged = false;
        }
        ImGui::SameLine(xOffset + width * 0.5f);
        if (ImGui::Button("Revert", ImVec2(width * 0.5f - padding * 4, 0)))
        {
            m_ImportOptions = m_OldImportOptions;
            m_HasPropertyChanged = false;
        }
        if (!changed)
            ImGui::EndDisabled();
    }

    void InspectorPanel::SetSelectedAssetPath(const Path& filepath)
    {
        if (filepath.empty())
        {
            SetInspectorMode(InspectorMode::Default);
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

        SpecificImporter* importer = Importer::Get().GetImporterForFile(filepath);
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
            else if (dynamic_cast<MeshImporter*>(importer))
                m_InspectorMode = InspectorMode::MeshImport;
            else
                m_InspectorMode = InspectorMode::Default;
        }
        else
        {
            m_InspectorMode = InspectorMode::Default;
        }

        Ref<LibraryEntry> entry = ProjectLibrary::Get().FindEntry(filepath);
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

    void InspectorPanel::SetSelectedEntity(Entity e)
    {
        m_InspectorMode = InspectorMode::GameObject;
        m_InspectedEntity = e;
        m_HasPropertyChanged = false;
    }

    void InspectorPanel::SetInspectorMode(InspectorMode mode)
    {
        m_InspectorMode = mode;
        m_HasPropertyChanged = false;
    }

} // namespace Crowny
