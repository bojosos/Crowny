#include "cwpch.h"

#include "Crowny/ImGui/OpenGLInformationWindow.h"
#include "Platform/OpenGL/OpenGlInfo.h"

#include <imgui.h>

namespace Crowny
{

	OpenGLInformationWindow::OpenGLInformationWindow(const std::string& name) : ImGuiWindow(name)
	{

	}

	void OpenGLInformationWindow::Render()
	{
		if (m_Shown) {
			ImGui::Begin("OpenGL Information");
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

	void OpenGLInformationWindow::Show()
	{
		m_Shown = true;
		if(OpenGLInfo::GetInformation().empty())
			OpenGLInfo::RetrieveInformation();
	}

	void OpenGLInformationWindow::Hide()
	{
		m_Shown = false;
	}
}