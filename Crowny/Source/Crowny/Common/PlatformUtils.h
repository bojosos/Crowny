#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    class PlatformUtils
    {
    public:
        static UUID42 GenerateUUID();
        static void ShowInExplorer(const Path& filepath);
        static void OpenExternally(const Path& filepath);
        static void CopyToClipboard(const String& string);
        static String CopyFromClipboard();
        static String Exec(const String& command);
    };

} // namespace Crowny