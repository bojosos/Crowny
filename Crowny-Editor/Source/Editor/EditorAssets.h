#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
    
    struct EditorAssetsLibrary
    {
        Ref<Texture2D> UnassignedTexture;
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
        static EditorAssetsLibrary s_Library;
    };

}