#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    enum class PlatformType
    {
        Windows,
        Linux,
        Mac,
        MacM1,
        Count
    };

    struct PlatformInfo
    {
        PlatformType Type;
        String Defines;
        UUID42 MainScene;
        bool Debug;
        AssetHandle<Texture> Icon;
    };
} // namespace Crowny