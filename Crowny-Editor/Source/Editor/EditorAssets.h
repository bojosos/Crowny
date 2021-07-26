#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
    
    struct EditorAssetsLibrary
    {
        // Ref<Texture2D> UnassignedTexture;
        // Ref<Texture2D> PlayIcon;
        // Ref<Texture2D> PauseIcon;
        // Ref<Texture2D> StopIcon;
    };

    class EditorAssets
    {
    public:

        static void Load();
        static EditorAssetsLibrary Get()
        {
            return s_Library;
        }

    private:
        static const std::string UnassignedTexture;
        static const std::string PlayIcon;
        static const std::string PauseIcon;
        static const std::string StopIcon;
        static EditorAssetsLibrary s_Library;
    };

}