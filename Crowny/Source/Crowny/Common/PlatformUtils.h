#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{

    class PlatformUtils
    {
    public:
        static Uuid GenerateUuid();
        static void ShowInExplorer(const std::string& filepath);
        static void OpenExternally(const std::string& filepath);
        static void CopyToClipboard(const std::string& string);
        static std::string CopyFromClipboard();
    };

} // namespace Crowny