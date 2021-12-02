#include "cwepch.h"

#include "Panels/GLInfoPanel.h"
#include "Platform/OpenGL/OpenGLInfo.h"

#include <imgui.h>

namespace Crowny
{

    OpenGLInformationPanel::OpenGLInformationPanel(const String& name) : ImGuiPanel(name) {}

    void OpenGLInformationPanel::Render()
    {
        if (OpenGLInfo::GetInformation().empty())
            OpenGLInfo::RetrieveInformation();
        CW_ENGINE_ASSERT(!OpenGLInfo::GetInformation().empty(), "OpenGL info error");

        ImGui::Begin("OpenGL Information", &m_Shown);
        UpdateState();
        ImGui::Columns(3, "OpenGL Information");
        ImGui::Separator();
        for (OpenGLDetail& det : OpenGLInfo::GetInformation())
        {
            ImGui::Text("%s", det.Name.c_str());
            ImGui::NextColumn();
            ImGui::Text("%s", det.GLName.c_str());
            ImGui::NextColumn();
            ImGui::Text("%s", det.Value.c_str());
            ImGui::NextColumn();
        }
        ImGui::Separator();
        ImGui::Columns(1);
        ImGui::End();
    }

} // namespace Crowny