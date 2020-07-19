#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"

namespace Crowny
{
	class ImGuiViewportWindow : public ImGuiWindow
	{
	public:
		ImGuiViewportWindow(const std::string& name);
		~ImGuiViewportWindow() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;
	private:
	};
}