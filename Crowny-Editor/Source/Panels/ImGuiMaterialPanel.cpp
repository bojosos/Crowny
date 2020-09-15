#include "cwepch.h"

#include "ImGuiMaterialPanel.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Common/FileSystem.h"

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
		
		if (s_SelectedMaterial)
		{

			if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> albedo = s_SelectedMaterial->GetAlbedoMap();
				if (albedo)
					ImGui::Image((ImTextureID)albedo->GetRendererID(), ImVec2(100, 100));
				else
					ImGui::Image((ImTextureID)Textures::Unassigned->GetRendererID(), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
					if (res)
					{
						s_SelectedMaterial->SetAlbedoMap(Texture2D::Create(path));
					}
				}

				//ImGui::Checkbox(s_SelectedMaterial->UseAlbedo);
				static glm::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
				if (ImGui::ColorEdit4("Color", glm::value_ptr(color), ImGuiColorEditFlags_NoInputs))
					s_SelectedMaterial->SetAlbedo(color);
			}

			if (ImGui::CollapsingHeader("Metalness", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> metalness = s_SelectedMaterial->GetMetalnessMap();
				if (metalness)
					ImGui::Image((ImTextureID)metalness->GetRendererID(), ImVec2(100, 100));
				else
					ImGui::Image((ImTextureID)Textures::Unassigned->GetRendererID(), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
					if (res)
					{
						s_SelectedMaterial->SetMetalnessMap(Texture2D::Create(path));
					}
				}

				//ImGui::Checkbox(s_SelectedMaterial->UseSpecular);
				static float metalnessVal = 0.8f;
				if (ImGui::SliderFloat("metalnessValue##Metalness", &metalnessVal, 0.0f, 1.0f))
				{
					s_SelectedMaterial->SetMetalness(metalnessVal);
				}
			}

			if (ImGui::CollapsingHeader("Normal", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> normal = s_SelectedMaterial->GetNormalMap();
				if (normal)
					ImGui::Image((ImTextureID)normal->GetRendererID(), ImVec2(100, 100));
				else
					ImGui::Image((ImTextureID)Textures::Unassigned->GetRendererID(), ImVec2(100, 100));
				
				if (ImGui::IsItemClicked())
				{
					auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
					if (res)
					{
						s_SelectedMaterial->SetNormalMap(Texture2D::Create(path));
					}
				}
				//ImGui::Checkbox(s_SelectedMaterial->UseSpecular);
			}

			if (ImGui::CollapsingHeader("Roughness", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> rougness = s_SelectedMaterial->GetRoughnessMap();
				if (rougness)
					ImGui::Image((ImTextureID)rougness->GetRendererID(), ImVec2(100, 100));
				else
					ImGui::Image((ImTextureID)Textures::Unassigned->GetRendererID(), ImVec2(100, 100));

				if (ImGui::IsItemClicked())
				{
					auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
					if (res)
					{
						s_SelectedMaterial->SetRougnessMap(Texture2D::Create(path));
					}
				}

				static float roughnessValue = 0.2f;
				if (ImGui::SliderFloat("rougnessValue##Roughness", &roughnessValue, 0.0f, 1.0f))
					s_SelectedMaterial->SetRougness(roughnessValue);

			}

			if (ImGui::CollapsingHeader("Ao Map", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Ref<Texture2D> ao = s_SelectedMaterial->GetAoMap();
				if (ao)
					ImGui::Image((ImTextureID)ao->GetRendererID(), ImVec2(100, 100));
				else
					ImGui::Image((ImTextureID)Textures::Unassigned->GetRendererID(), ImVec2(100, 100));
				
				if (ImGui::IsItemClicked())
				{
					auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
					if (res)
					{
						s_SelectedMaterial->SetAoMap(Texture2D::Create(path));
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