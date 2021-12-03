#pragma once

#include <imgui.h>

namespace Crowny
{

    class UIUtils
    {
    public:
        /**
         * @brief A simple Yes/No message box. You have to call ImGui::OpenPopup with the title.
         *
         * @param title Title and also Id that should be used in ImGui::OpenPopup
         * @param message
         * @return true
         * @return false
         */
        static bool ShowYesNoMessageBox(const String& title, const String& message);

        struct ScopedDisable
        { 
            ScopedDisable(bool disabled) : m_Disable(disabled) { if (m_Disable) ImGui::BeginDisabled(); }
            ~ScopedDisable() { if (m_Disable) ImGui::EndDisabled(); } bool m_Disable;
        };
    };

} // namespace Crowny