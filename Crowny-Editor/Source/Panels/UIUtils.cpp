#include "cwepch.h"

#include "Panels/UIUtils.h"

#include <imgui.h>

namespace Crowny
{
    bool UIUtils::ShowYesNoMessageBox(const String& title, const String& message)
    {
        // if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            if (ImGui::Button("Yes"))
            {
                ImGui::CloseCurrentPopup();
                return true;
            }
            if (ImGui::Button("No"))
            {
                ImGui::CloseCurrentPopup();
                return false;
            }
            ImGui::EndPopup();
        }
        return false;
    }
} // namespace Crowny