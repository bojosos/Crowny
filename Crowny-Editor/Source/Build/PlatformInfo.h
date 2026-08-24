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

    struct PlatformInfo : public RefCounted
    {
        PlatformType Type = PlatformType::Windows;
        String Defines;
        UUID MainScene;
        Path OutputDirectory;
        bool Debug = false;
        bool ExportSupported = false;
        AssetHandle<Texture> Icon;
    };
} // namespace Crowny
