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
        Ref<Texture> ArrowPointerIcon;
        Ref<Texture> ArrowsIcon;
        Ref<Texture> RotateIcon;
        Ref<Texture> MaximizeIcon;
        Ref<Texture> GlobeIcon;
        Ref<Texture> SearchIcon;
        Ref<Texture> ConsoleInfo;
        Ref<Texture> ConsoleWarn;
        Ref<Texture> ConsoleError;
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

        static const String ArrowPointerIcon;
        static const String ArrowsIcon;
        static const String RotateIcon;
        static const String MaximizeIcon;
        static const String GlobeIcon;
        static const String SearchIcon;

        static const String ConsoleInfo;
        static const String ConsoleWarn;
        static const String ConsoleError;

        static EditorAssetsLibrary s_Library;
    };

} // namespace Crowny