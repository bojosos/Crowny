#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Scene/SceneManager.h"

#include "Panels/ComponentEditor.h"
#include "Panels/InspectorPanel.h"

#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "Panels/UIUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace Crowny
{

    Ref<PBRMaterial> InspectorPanel::s_SelectedMaterial = nullptr;

    InspectorPanel::InspectorPanel(const String& name) : ImGuiPanel(name)
    {
        m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");

        // Rendering
        m_ComponentEditor.PushComponentGroup("Rendering");
        m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
        m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
        m_ComponentEditor.RegisterComponent<TextComponent>("Text");
        m_ComponentEditor.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
        m_ComponentEditor.PopComponentGroup();
        // Scripting
        m_ComponentEditor.RegisterComponent<MonoScriptComponent>("C# Script");

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
    }

    void InspectorPanel::Render()
    {
        ImGui::Begin("Inspector", &m_Shown);
        UpdateState();

        DrawHeader();
        if (m_InspectorMode == InspectorMode::GameObject)
            m_ComponentEditor.Render();
        else if (m_InspectorMode == InspectorMode::Material)
            RenderMaterialInspector();
        else if (m_InspectorMode == InspectorMode::PhysicsMaterial)
            RenderPhysicsMaterialInspector();
        else if (m_InspectorMode == InspectorMode::AudioClipImport)
            RenderAudioClipImportInspector();
        else if (m_InspectorMode == InspectorMode::FontImport)
            RenderFontImportInspector();
        else if (m_InspectorMode == InspectorMode::ScriptImport)
            RenderScriptImportInspector();
        else if (m_InspectorMode == InspectorMode::MeshImport)
            RenderMeshImportInspector();
        else if (m_InspectorMode == InspectorMode::Prefab)
            RenderPrefabInspector();
        // else
        //     CW_ENGINE_ASSERT(false, "Invalid inspector mode");

        ImGui::End();
    }

    void InspectorPanel::RenderMaterialInspector()
    {
        if (s_SelectedMaterial)
        {
            if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Ref<Texture> albedo = s_SelectedMaterial->GetAlbedoMap();
                if (albedo)
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(albedo), ImVec2(100, 100));
                else
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), ImVec2(100, 100));

                if (ImGui::IsItemClicked())
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
                    {
                        Ref<Texture> albedo;
                        // LoadTexture(outPaths[0], albedo);
                        s_SelectedMaterial->SetAlbedoMap(albedo);
                    }
                }

                static glm::vec4 color = glm::vec4(1.0f);
                if (ImGui::ColorEdit4("Color", glm::value_ptr(color), ImGuiColorEditFlags_NoInputs))
                    s_SelectedMaterial->SetAlbedo(color);

                if (ImGui::Button("Reset##resetAlbedo"))
                {
                    s_SelectedMaterial->SetAlbedoMap(nullptr);
                    s_SelectedMaterial->SetAlbedo(glm::vec4(1.0f));
                    color = glm::vec4(1.0f);
                }
            }

            if (ImGui::CollapsingHeader("Metalness", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Ref<Texture> metalness = s_SelectedMaterial->GetMetalnessMap();
                if (metalness)
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(metalness), ImVec2(100, 100));
                else
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), ImVec2(100, 100));

                if (ImGui::IsItemClicked())
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
                    {
                        Ref<Texture> metalness;
                        //  LoadTexture(outPaths[0], metalness);
                        s_SelectedMaterial->SetMetalnessMap(metalness);
                    }
                }

                static float metalnessVal = 0.8f;
                if (ImGui::SliderFloat("Metalness##metalnessValue", &metalnessVal, 0.0f, 1.0f))
                {
                    s_SelectedMaterial->SetMetalness(metalnessVal);
                }

                if (ImGui::Button("Reset##resetMetalness"))
                {
                    metalnessVal = 0.8f;
                    s_SelectedMaterial->SetMetalnessMap(nullptr);
                    s_SelectedMaterial->SetMetalness(0.8);
                }
            }

            if (ImGui::CollapsingHeader("Normal", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Ref<Texture> normal = s_SelectedMaterial->GetNormalMap();
                if (normal)
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(normal), ImVec2(100, 100));
                else
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), ImVec2(100, 100));

                if (ImGui::IsItemClicked())
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
                    {
                        Ref<Texture> normal;
                        // LoadTexture(outPaths[0], normal);
                        s_SelectedMaterial->SetNormalMap(normal);
                    }
                }
                if (ImGui::Button("Reset##resetNormal"))
                {
                    s_SelectedMaterial->SetNormalMap(nullptr);
                }
            }

            if (ImGui::CollapsingHeader("Roughness", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Ref<Texture> roughness = s_SelectedMaterial->GetRoughnessMap();
                if (roughness)
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(roughness), ImVec2(100, 100));
                else
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), ImVec2(100, 100));

                if (ImGui::IsItemClicked())
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
                    {
                        Ref<Texture> roughness;
                        // LoadTexture(outPaths[0], roughness);
                        s_SelectedMaterial->SetRoughnessMap(roughness);
                    }
                }

                static float roughnessValue = 0.2f;
                if (ImGui::SliderFloat("rougnessValue##Roughness", &roughnessValue, 0.0f, 1.0f))
                    s_SelectedMaterial->SetRoughness(roughnessValue);

                if (ImGui::Button("Reset##resetRoughness"))
                {
                    roughnessValue = 0.2f;
                    s_SelectedMaterial->SetRoughnessMap(nullptr);
                    s_SelectedMaterial->SetRoughness(0.2f);
                }
            }

            if (ImGui::CollapsingHeader("Ao Map", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Ref<Texture> ao = s_SelectedMaterial->GetAoMap();
                if (ao)
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(ao), ImVec2(100, 100));
                else
                    ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), ImVec2(100, 100));

                if (ImGui::IsItemClicked())
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
                    {
                        Ref<Texture> ao;
                        // LoadTexture(outPaths[0], ao);
                        s_SelectedMaterial->SetAoMap(ao);
                    }
                }
            }
        }
    }

    void InspectorPanel::RenderPhysicsMaterialInspector() {}

    void InspectorPanel::RenderAudioClipImportInspector()
    {
        if (m_ImportOptions)
        {
            Ref<AudioClipImportOptions> audioClipImport =
              std::static_pointer_cast<AudioClipImportOptions>(m_ImportOptions);
            ImGui::Columns(2);
            ImGui::Text("Format");
            ImGui::NextColumn();
            const char* formatTexts[2] = { "PCM", "Vorbis" };
            uint32_t formatIndex = (uint32_t)audioClipImport->Format;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AudioFormat", formatTexts[formatIndex]))
            {
                for (uint32_t i = 0; i < 2; i++)
                {
                    const bool is_selected = (formatIndex == i);
                    if (ImGui::Selectable(formatTexts[i], is_selected))
                    {
                        if (audioClipImport->Format != (AudioFormat)i)
                            m_HasPropertyChanged = true;
                        audioClipImport->Format = (AudioFormat)i;
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::NextColumn();
            ImGui::Text("Load Mode");
            ImGui::NextColumn();
            const char* loadModeTexts[3] = { "Load Decompressed", "Load Compressed", "Stream" };
            uint32_t loadModeIndex = (uint32_t)audioClipImport->ReadMode;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AudioLoadMode", loadModeTexts[loadModeIndex]))
            {
                for (uint32_t i = 0; i < 3; i++)
                {
                    const bool is_selected = (loadModeIndex == i);
                    if (ImGui::Selectable(loadModeTexts[i], is_selected))
                    {
                        if (audioClipImport->ReadMode != (AudioReadMode)i)
                            m_HasPropertyChanged = true;
                        audioClipImport->ReadMode = (AudioReadMode)i;
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::NextColumn();
            ImGui::Text("Bit Depth");
            ImGui::NextColumn();
            const char* bitDepthTexts[4] = { "8", "16", "24", "32" };
            uint32_t bitDepthIndex = (uint32_t)audioClipImport->BitDepth / 8 - 1;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AudioBitDepth", bitDepthTexts[bitDepthIndex]))
            {
                for (uint32_t i = 0; i < 4; i++)
                {
                    const bool is_selected = (bitDepthIndex == i);
                    if (ImGui::Selectable(bitDepthTexts[i], is_selected))
                    {
                        if (audioClipImport->BitDepth != (i + 1) * 8)
                            m_HasPropertyChanged = true;
                        audioClipImport->BitDepth = (i + 1) * 8;
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::NextColumn();
            ImGui::Text("3D");
            ImGui::NextColumn();
            float x = ImGui::GetCursorPosX();
            float width = ImGui::GetColumnWidth();
            if (ImGui::Checkbox("##IsClip3D", &audioClipImport->Is3D))
                m_HasPropertyChanged = true;
            ImGui::NextColumn();
            ImGui::NextColumn();
            ImGui::Columns(1);
            DrawApplyRevert(x, width);
        }
    }

    void InspectorPanel::RenderFontImportInspector() {}

    void InspectorPanel::RenderScriptImportInspector() {}

    void InspectorPanel::RenderTextureImportInspector() {}

    void InspectorPanel::RenderShaderImportInspector() {}

    void InspectorPanel::RenderMeshImportInspector() {}

    void InspectorPanel::RenderPrefabInspector() {}

    void InspectorPanel::DrawHeader()
    {
        // Consider drawing an icon too
        auto drawHeader = [&](const String& head) {
            float maxx = ImGui::GetContentRegionAvail().x;
            ImGui::Text("%s", (m_InspectedAssetPath.filename().string() + " (" + head + ") Import Settings").c_str());
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
        case (InspectorMode::AudioClipImport):
            drawHeader("Audio Clip");
            break;
        case (InspectorMode::TextureImport):
            drawHeader("Texture");
            break;
        case (InspectorMode::FontImport):
            drawHeader("Font");
            break;
        case (InspectorMode::PhysicsMaterial):
            drawHeader("Physics Material");
            break;
        case (InspectorMode::ScriptImport):
            drawHeader("C# script");
            break;
        // case (InspectorMode::GameObject):   drawHeader("Texture"); break; This should be different
        case (InspectorMode::ShaderImport):
            drawHeader("Shader");
            break;
        case (InspectorMode::MeshImport):
            drawHeader("Mesh");
            break;
        case (InspectorMode::Material):
            drawHeader("Material");
            break;
        default: {
            float maxx = ImGui::GetContentRegionAvail().x;
            ImGui::Text("%s", m_InspectedAssetPath.filename().c_str());
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

    void InspectorPanel::DrawApplyRevert(float xOffset, float width)
    {
        ImGui::Separator();
        float padding = ImGui::GetStyle().FramePadding.x;
        ImGui::SetCursorPosX(xOffset);
        bool changed = m_HasPropertyChanged;
        if (!changed)
            ImGui::BeginDisabled();
        if (ImGui::Button("Apply", ImVec2(width * 0.5f - padding * 4, 0)))
            ProjectLibrary::Get().Reimport(m_InspectedAssetPath, m_ImportOptions, true);
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
        m_InspectedAssetPath = filepath;
        if (fs::is_directory(filepath))
        {
            m_InspectorMode = InspectorMode::Default;
            return;
        }
        String ext = filepath.extension();
        ext = ext.substr(1, ext.size() - 1);
        if (ext == "ogg" || ext == "wav" || ext == "flac")
            m_InspectorMode = InspectorMode::AudioClipImport;
        else if (ext == "png" || ext == "jpeg" || ext == "psd" || ext == "gif" || ext == "tga" || ext == "bmp" ||
                 ext == "hdr")
            m_InspectorMode = InspectorMode::TextureImport;
        else
            m_InspectorMode = InspectorMode::Default;
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
    }

    void InspectorPanel::SetInspectorMode(InspectorMode mode) { m_InspectorMode = mode; }

    void InspectorPanel::Show() { m_Shown = true; }

    void InspectorPanel::Hide() { m_Shown = false; }

} // namespace Crowny
