#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{

    struct EditorAssetsLibrary
    {
        Ref<Texture> UnassignedTexture;
        Ref<Texture> PlayIcon;
        Ref<Texture> PauseIcon;
        Ref<Texture> StopIcon;
        Ref<Texture> FileIcon;
        Ref<Texture> FolderIcon;
    };

    class EditorAssets
    {
    public:
        static void Load();
        static EditorAssetsLibrary Get() { return s_Library; }

        static const std::string DefaultScriptPath;

    private:
        static const std::string UnassignedTexture;
        static const std::string PlayIcon;
        static const std::string PauseIcon;
        static const std::string StopIcon;

        static const std::string FileIcon;
        static const std::string FolderIcon;

        static EditorAssetsLibrary s_Library;
    };

} // namespace Crowny