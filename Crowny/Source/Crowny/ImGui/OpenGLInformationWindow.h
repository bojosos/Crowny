#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"

namespace Crowny
{
	class OpenGLInformationWindow : public ImGuiWindow
	{
	public:
		OpenGLInformationWindow(const std::string& name);
		~OpenGLInformationWindow() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;
	};
}