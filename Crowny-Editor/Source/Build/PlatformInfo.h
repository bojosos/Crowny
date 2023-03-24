#pragma once

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Assets/AssetHandle.h"

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
        UUID MainScene;
        bool Debug;
        AssetHandle<Texture> Icon;
	};
}