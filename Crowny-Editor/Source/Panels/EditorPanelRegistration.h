#pragma once

#include "Crowny/Common/Common.h"

namespace Crowny
{
    struct EditorPanelMetadata
    {
        StringView Name;
        StringView MenuPath;
        StringView Shortcut;
        bool OpenByDefault = true;
    };

    template <typename T> struct EditorPanelRegistration
    {
        constexpr EditorPanelRegistration(StringView name, StringView menuPath, StringView shortcut = {}, bool openByDefault = true)
          : Metadata{ name, menuPath, shortcut, openByDefault }
        {
        }

        EditorPanelMetadata Metadata;
    };
} // namespace Crowny
