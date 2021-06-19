#pragma once

#include "ImGuiPanel.h"

namespace Crowny
{
	class OpenGLInformationPanel : public ImGuiPanel
	{
	public:
		OpenGLInformationPanel(const std::string& name);
		~OpenGLInformationPanel() = default;

		virtual void Render() override;
	};
}