#include "cwepch.h"

#include "ImGuiMaterialPanel.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Editor/EditorAssets.h"

#include <imgui.h>
#include "glm/gtc/type_ptr.inl"

namespace Crowny
{

	Ref<PBRMaterial> ImGuiMaterialPanel::s_SelectedMaterial = nullptr;

	ImGuiMaterialPanel::ImGuiMaterialPanel(const std::string& name) : ImGuiPanel(name)
	{

	}

	void ImGuiMaterialPanel::Render()
	{
		ImGui::Begin("Material", &m_Shown);
		UpdateState();
		
		if (s_SelectedMaterial)
		{

			if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> albedo = s_SelectedMaterial->GetAlbedoMap();
				if (albedo)
					ImGui::Image(reinterpret_cast<void*>(albedo->GetRendererID()), ImVec2(100, 100));
				else
					ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					std::vector<std::string> outPaths;
					if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
					{
						s_SelectedMaterial->SetAlbedoMap(Texture2D::Create(outPaths[0]));
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
				Ref<Texture2D> metalness = s_SelectedMaterial->GetMetalnessMap();
				if (metalness)
					ImGui::Image(reinterpret_cast<void*>(metalness->GetRendererID()), ImVec2(100, 100));
				else
					ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					std::vector<std::string> outPaths;
					if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
					{
						s_SelectedMaterial->SetMetalnessMap(Texture2D::Create(outPaths[0]));
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
				Ref<Texture2D> normal = s_SelectedMaterial->GetNormalMap();
				if (normal)
					ImGui::Image(reinterpret_cast<void*>(normal->GetRendererID()), ImVec2(100, 100));
				else
					ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), ImVec2(100, 100));
				
				if (ImGui::IsItemClicked())
				{
					std::vector<std::string> outPaths;
					if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
					{
						s_SelectedMaterial->SetNormalMap(Texture2D::Create(outPaths[0]));
					}
				}
				if (ImGui::Button("Reset##resetNormal"))
				{
					s_SelectedMaterial->SetNormalMap(nullptr);
				}
			}

			if (ImGui::CollapsingHeader("Roughness", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> rougness = s_SelectedMaterial->GetRoughnessMap();
				if (rougness)
					ImGui::Image(reinterpret_cast<void*>(rougness->GetRendererID()), ImVec2(100, 100));
				else
					ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					std::vector<std::string> outPaths;
					if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
					{
						s_SelectedMaterial->SetRoughnessMap(Texture2D::Create(outPaths[0]));
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
				Ref<Texture2D> ao = s_SelectedMaterial->GetAoMap();
				if (ao)
					ImGui::Image(reinterpret_cast<void*>(ao->GetRendererID()), ImVec2(100, 100));
				else
					ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), ImVec2(100, 100));
				
				if (ImGui::IsItemClicked())
				{
					std::vector<std::string> outPaths;
					if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
					{
						s_SelectedMaterial->SetAoMap(Texture2D::Create(outPaths[0]));
					}
				}
			}
		}

		ImGui::End();
	}

	void ImGuiMaterialPanel::Show()
	{
		m_Shown = true;
	}

	void ImGuiMaterialPanel::Hide()
	{
		m_Shown = false;
	}
}