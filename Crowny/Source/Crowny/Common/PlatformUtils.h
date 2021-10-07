#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    class PlatformUtils
    {
    public:
        static UUID GenerateUUID();
        static void ShowInExplorer(const Path& filepath);
        static void OpenExternally(const Path& filepath);
        static void CopyToClipboard(const String& string);
        static String CopyFromClipboard();
    };

} // namespace Crowny