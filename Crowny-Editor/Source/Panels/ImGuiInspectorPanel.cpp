#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Scene/SceneManager.h"

#include "Panels/ImGuiComponentEditor.h"
#include "Panels/ImGuiInspectorPanel.h"

#include "Editor/EditorAssets.h"

#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.inl>
#include <imgui.h>

#include <imgui.h>

namespace Crowny
{

    Ref<PBRMaterial> ImGuiInspectorPanel::s_SelectedMaterial = nullptr;

    ImGuiInspectorPanel::ImGuiInspectorPanel(const std::string& name) : ImGuiPanel(name)
    {
        m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");
        m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
        m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
        m_ComponentEditor.RegisterComponent<TextComponent>("Text");
        m_ComponentEditor.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
        m_ComponentEditor.RegisterComponent<MonoScriptComponent>("C# Script");

        m_ComponentEditor.RegisterComponent<AudioListenerComponent>("Audio Listener");
        m_ComponentEditor.RegisterComponent<AudioSourceComponent>("Audio Source");
    }

    void ImGuiInspectorPanel::Render()
    {
        ImGui::Begin("Inspector", &m_Shown);
        UpdateState();

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
        else
            CW_ENGINE_ASSERT(false, "Invalid inspector mode");

        ImGui::End();
    }

    void ImGuiInspectorPanel::RenderMaterialInspector()
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
                    std::vector<std::string> outPaths;
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
                    std::vector<std::string> outPaths;
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
                    std::vector<std::string> outPaths;
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
                    std::vector<std::string> outPaths;
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
                    std::vector<std::string> outPaths;
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

    void ImGuiInspectorPanel::RenderPhysicsMaterialInspector() {}

    void ImGuiInspectorPanel::RenderAudioClipImportInspector()
    {
        if (m_ImportOptions)
        {
            Ref<AudioClipImportOptions> audioClipImport =
              std::static_pointer_cast<AudioClipImportOptions>(m_ImportOptions);

            ImGui::Columns(2);
            ImGui::Text("Format");
            ImGui::SameLine();
            const char* formatTexts[2] = { "PCM", "Vorbis" };
            uint32_t formatIndex = (uint32_t)audioClipImport->Format;
            if (ImGui::BeginCombo("##AudioFormat", formatTexts[formatIndex]))
            {
                for (uint32_t i = 0; i < 2; i++)
                {
                    const bool is_selected = (formatIndex == i);
                    if (ImGui::Selectable(formatTexts[i], is_selected))
                        audioClipImport->Format = (AudioFormat)formatIndex;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Text("Load Mode");
            ImGui::SameLine();
            const char* loadModeTexts[3] = { "Load Decompressed", "Load Compressed", "Stream" };
            uint32_t loadModeIndex = (uint32_t)audioClipImport->ReadMode;
            if (ImGui::BeginCombo("##AudioLoadMode", loadModeTexts[loadModeIndex]))
            {
                for (uint32_t i = 0; i < 3; i++)
                {
                    const bool is_selected = (loadModeIndex == i);
                    if (ImGui::Selectable(loadModeTexts[i], is_selected))
                        audioClipImport->ReadMode = (AudioReadMode)loadModeIndex;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Text("Bit Depth");
            ImGui::SameLine();
            const char* bitDepthTexts[4] = { "8", "16", "24", "32" };
            uint32_t bitDepthIndex = (uint32_t)audioClipImport->BitDepth / 8 - 1;
            if (ImGui::BeginCombo("##AudioBitDepth", loadModeTexts[bitDepthIndex]))
            {
                for (uint32_t i = 0; i < 4; i++)
                {
                    const bool is_selected = (bitDepthIndex == i);
                    if (ImGui::Selectable(loadModeTexts[i], is_selected))
                        audioClipImport->BitDepth = (bitDepthIndex + 1) * 8;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Text("3D");
            ImGui::SameLine();
            ImGui::Checkbox("##IsClip3D", &audioClipImport->Is3D);

            if (ImGui::Button("Apply"))
            {
                Importer::Get().Reimport(m_InspectedAssetPath, m_ImportOptions);
            }
            if (ImGui::Button("Revert"))
            {
                m_ImportOptions = m_OldImportOptions;
            }
        }
    }

    void ImGuiInspectorPanel::RenderFontImportInspector() {}

    void ImGuiInspectorPanel::RenderScriptImportInspector() {}

    void ImGuiInspectorPanel::RenderTextureImportInspector() {}

    void ImGuiInspectorPanel::RenderShaderImportInspector() {}

    void ImGuiInspectorPanel::RenderMeshImportInspector() {}

    void ImGuiInspectorPanel::RenderPrefabInspector() {}

    void ImGuiInspectorPanel::SetSelectedAssetPath(const std::string& filepath)
    {
        // m_ImportOptions =
        // ProjectManager::Get().GetActiveProject().GetFileEntry(m_InspectedAssetPath).Meta->GetOptions();
        // m_OldImportOptions = m_ImportOptions->Clone();
    }

    void ImGuiInspectorPanel::SetInspectorMode(InspectorMode mode) { m_InspectorMode = mode; }

    void ImGuiInspectorPanel::Show() { m_Shown = true; }

    void ImGuiInspectorPanel::Hide() { m_Shown = false; }

} // namespace Crowny