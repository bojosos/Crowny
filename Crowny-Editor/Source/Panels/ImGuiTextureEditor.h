#pragma once

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{

	class ImGuiTextureEditor : public ImGuiPanel
	{
	public:
		ImGuiTextureEditor(const std::string& name) : ImGuiPanel(name) { }
		virtual ~ImGuiTextureEditor() = default;

		virtual void Render() override;
	
	public:
		static void SetTexture(const Ref<Texture2D>& texture);

	private:
		static Ref<Texture> s_Texture;
	};
}