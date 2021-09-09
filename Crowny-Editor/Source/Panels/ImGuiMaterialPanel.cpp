#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Editor/EditorAssets.h"
#include "ImGuiMaterialPanel.h"

#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.inl>
#include <imgui.h>

namespace Crowny
{
    extern void LoadTexture(const std::string& path, Ref<Texture>& out);
    Ref<PBRMaterial> ImGuiMaterialPanel::s_SelectedMaterial = nullptr;

    ImGuiMaterialPanel::ImGuiMaterialPanel(const std::string& name) : ImGuiPanel(name) {}

    void ImGuiMaterialPanel::Render()
    {
        ImGui::Begin("Material", &m_Shown);
        UpdateState();

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
                        LoadTexture(outPaths[0], albedo);
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
                        LoadTexture(outPaths[0], metalness);
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
                        LoadTexture(outPaths[0], normal);
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
                        LoadTexture(outPaths[0], roughness);
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
                        LoadTexture(outPaths[0], ao);
                        s_SelectedMaterial->SetAoMap(ao);
                    }
                }
            }
        }

        ImGui::End();
    }

    void ImGuiMaterialPanel::Show() { m_Shown = true; }

    void ImGuiMaterialPanel::Hide() { m_Shown = false; }
} // namespace Crowny