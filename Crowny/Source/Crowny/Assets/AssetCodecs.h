#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    struct AssetFileHeader;

    void InitializeAssetCodecs();
    bool PeekAssetHeader(const Path& assetPath, AssetFileHeader& outHeader);
} // namespace Crowny
