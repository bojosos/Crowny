#include "cwepch.h"

#include "OpenGLInformationPanel.h"
#include "Platform/OpenGL/OpenGlInfo.h"

#include <imgui.h>

namespace Crowny
{

	OpenGLInformationPanel::OpenGLInformationPanel(const std::string& name) : ImGuiPanel(name)
	{

	}

	void OpenGLInformationPanel::Render()
	{
		if (OpenGLInfo::GetInformation().empty())
			OpenGLInfo::RetrieveInformation();
		CW_ENGINE_ASSERT(!OpenGLInfo::GetInformation().empty(), "OpenGL info error");

		ImGui::Begin("OpenGL Information", &m_Shown);
		ImGui::Columns(3, "OpenGL Information");
		ImGui::Separator();
		for (OpenGLDetail& det : OpenGLInfo::GetInformation())
		{
			ImGui::Text(det.Name.c_str()); ImGui::NextColumn();
			ImGui::Text(det.GLName.c_str()); ImGui::NextColumn();
			ImGui::Text(det.Value.c_str()); ImGui::NextColumn();
		}
		ImGui::Separator();
		ImGui::Columns(1);
		ImGui::End();
	}

}