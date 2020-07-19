#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"

namespace Crowny
{
	// Takes the objects from the active scene and dispalys them
	class ImGuiHierarchyWindow : public ImGuiWindow
	{
	public:
		ImGuiHierarchyWindow(const std::string& name);
		~ImGuiHierarchyWindow() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;
	};
}