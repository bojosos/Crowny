#pragma once

namespace Crowny
{

    class ImGuiUtils
    {
        /**
         * @brief A simple Yes/No message box. You have to call ImGui::OpenPopup with the title.
         *
         * @param title Title and also Id that should be used in ImGui::OpenPopup
         * @param message
         * @return true
         * @return false
         */
        static bool ShowYesNoMessageBox(const String& title, const String& message);
    };

} // namespace Crowny