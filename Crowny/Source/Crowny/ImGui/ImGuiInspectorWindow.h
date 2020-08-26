#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"
#include "Crowny/ImGui/ImGuiComponentEditor.h"

namespace Crowny
{
	class Entity;

	class ImGuiInspectorWindow : public ImGuiWindow
	{
	public:
		ImGuiInspectorWindow(const std::string& name);
		~ImGuiInspectorWindow() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;

	private:
		ImGuiComponentEditor m_ComponentEditor;
	};
}