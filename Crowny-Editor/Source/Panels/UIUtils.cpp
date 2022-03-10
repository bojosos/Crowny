#include "cwepch.h"

#include "Panels/UIUtils.h"
#include "Editor/EditorDefaults.h"

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

    bool UIUtils::DrawFloatControl(float& value, float minValue, float maxValue, bool asSlider)
    {
        if (asSlider)
            return ImGui::SliderFloat("##sliderFloat", &value, minValue, maxValue);
        else
            return ImGui::DragFloat("##dragFloatScript", &value, DRAG_SENSITIVITY, minValue, maxValue);
    }
} // namespace Crowny