#pragma once

#include "ImGuiPanel.h"

#include <imgui.h>
#include <vector>
#include <string>

namespace Crowny
{

	class ImGuiTextureEditor : public ImGuiPanel
	{
	public:
		ImGuiTextureEditor(const std::string& name) : ImGuiPanel(name) { }
		virtual ~ImGuiTextureEditor() = default;

		const std::vector<const char*> TextureTypes = { "Default", "Normal", "Sprite", "Cursor", "Lightmap", "Single channel" };
		uint32_t SelectedTextureType = 0;

		const std::vector<const char*> TextureShapes = { "2D", "Cube" };
		uint32_t SelectedTextureShape = 0;

		bool sRGB = true;

		const std::vector<const char*> AlphaSources = { "From Grayscale", "Input Texture Alpha" };
		uint32_t m_SelectedAlphaSource = 0;

		bool ReadWrite = false;

		bool GenerateMipmaps = false;

		const std::vector<const char*> WrapModes = { "Repeat", "Clamp", "Mirror", "Mirror Once" };
		uint32_t SelectedWrapMode = 0;

		const std::vector<const char*> FilterModes = { "Point", "Bilinear", "Trilinear" };
		uint32_t SelectedFilterMode = 0;

		uint32_t AnsioLevel;

		uint32_t MaxSize;

		const std::vector<const char*> ResizeAlgorithms = { "Mitchell", "Bilinear" };
		uint32_t SelectedResizeAlgorithm = 0;

		const std::vector<const char*> CompressionQualities = { "None", "Low Quality", "Normal Quality", "High Quality" };
		uint32_t SelectedCompressionQuality = 0;

		void Render() override
		{
			ImGui::Begin("Texture Properties");
			if (ImGui::BeginCombo("Texture type", TextureTypes[SelectedTextureType]))
			{
				for (uint32_t n = 0; n < TextureTypes.size(); n++)
				{
					const bool is_selected = (SelectedTextureType == n);
					if (ImGui::Selectable(TextureTypes[n], is_selected))
						SelectedTextureType = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("Texture shape", TextureShapes[SelectedTextureShape]))
			{
				for (int n = 0; n < TextureShapes.size(); n++)
				{
					const bool is_selected = (SelectedTextureShape == n);
					if (ImGui::Selectable(TextureShapes[n], is_selected))
						SelectedTextureShape = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Checkbox("sRGB", &sRGB);

			if (ImGui::BeginCombo("Alpha source", AlphaSources[m_SelectedAlphaSource]))
			{
				for (int n = 0; n < AlphaSources.size(); n++)
				{
					const bool is_selected = (m_SelectedAlphaSource == n);
					if (ImGui::Selectable(AlphaSources[n], is_selected))
						m_SelectedAlphaSource = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Checkbox("Enable Read/Write", &ReadWrite);

			ImGui::Checkbox("Generate Mipmaps", &GenerateMipmaps);

			if (ImGui::BeginCombo("Wrap Mode", WrapModes[SelectedWrapMode]))
			{
				for (int n = 0; n < WrapModes.size(); n++)
				{
					const bool is_selected = (SelectedWrapMode == n);
					if (ImGui::Selectable(WrapModes[n], is_selected))
						SelectedWrapMode = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("Filter Mode", FilterModes[SelectedFilterMode]))
			{
				for (int n = 0; n < FilterModes.size(); n++)
				{
					const bool is_selected = (SelectedFilterMode == n);
					if (ImGui::Selectable(FilterModes[n], is_selected))
						SelectedFilterMode = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("Resize Algorithm", ResizeAlgorithms[SelectedResizeAlgorithm]))
			{
				for (int n = 0; n < ResizeAlgorithms.size(); n++)
				{
					const bool is_selected = (SelectedResizeAlgorithm == n);
					if (ImGui::Selectable(ResizeAlgorithms[n], is_selected))
						SelectedResizeAlgorithm = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("Compression Quality", CompressionQualities[SelectedCompressionQuality]))
			{
				for (int n = 0; n < CompressionQualities.size(); n++)
				{
					const bool is_selected = (SelectedCompressionQuality == n);
					if (ImGui::Selectable(CompressionQualities[n], is_selected))
						SelectedCompressionQuality = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::End();
		}

	};
}