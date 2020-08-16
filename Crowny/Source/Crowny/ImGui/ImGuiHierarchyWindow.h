#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"

namespace Crowny
{
	class Entity;
		
	class ImGuiHierarchyWindow : public ImGuiWindow
	{
	public:
		ImGuiHierarchyWindow(const std::string& name);
		~ImGuiHierarchyWindow() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;
		static Entity SelectedEntity;

	private:
		void DisplayTree(Entity& e, uint32_t i = 0);
	};
}