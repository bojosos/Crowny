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
        static void Unload();
        static EditorAssetsLibrary Get() { return s_Library; }

        static const String DefaultScriptPath;

    private:
        static const String UnassignedTexture;
        static const String PlayIcon;
        static const String PauseIcon;
        static const String StopIcon;

        static const String FileIcon;
        static const String FolderIcon;

        static EditorAssetsLibrary s_Library;
    };

} // namespace Crowny