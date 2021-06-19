#pragma once

#include "ImGuiPanel.h"
#include "ImGuiComponentEditor.h"

namespace Crowny
{
	class Entity;

	class ImGuiInspectorPanel : public ImGuiPanel
	{
	public:
		ImGuiInspectorPanel(const std::string& name);
		~ImGuiInspectorPanel() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;

	private:
		ImGuiComponentEditor m_ComponentEditor;
	};
}