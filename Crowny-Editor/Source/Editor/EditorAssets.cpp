#include "cwepch.h"

#include "Editor/EditorAssets.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    const String EditorAssets::UnassignedTexture = "Resources/Textures/Unassigned.png";

    const String EditorAssets::PlayIcon = "Resources/Icons/Play.png";
    const String EditorAssets::PauseIcon = "Resources/Icons/Pause.png";
    const String EditorAssets::StopIcon = "Resources/Icons/Stop.png";

    const String EditorAssets::FileIcon = "Resources/Icons/File.png";
    const String EditorAssets::FolderIcon = "Resources/Icons/Folder.png";

    const String EditorAssets::ArrowPointerIcon = "Resources/Icons/ArrowPointerIcon.png";
    const String EditorAssets::ArrowsIcon = "Resources/Icons/ArrowsIcon.png";
    const String EditorAssets::RotateIcon = "Resources/Icons/RotateIcon.png";
    const String EditorAssets::MaximizeIcon = "Resources/Icons/MaximizeIcon.png";
    const String EditorAssets::GlobeIcon = "Resources/Icons/GlobeIcon.png";
    const String EditorAssets::SearchIcon = "Resources/Icons/SearchIcon.png";

    const String EditorAssets::ConsoleInfo = "Resources/Icons/ConsoleInfo.png";
    const String EditorAssets::ConsoleWarn = "Resources/Icons/ConsoleWarn.png";
    const String EditorAssets::ConsoleError = "Resources/Icons/ConsoleError.png";

    const String EditorAssets::AlignLeft = "Resources/Icons/AlignLeft.png";
    const String EditorAssets::AlignCenter = "Resources/Icons/AlignCenter.png";
    const String EditorAssets::AlignRight = "Resources/Icons/AlignRight.png";

    const String EditorAssets::DefaultScriptPath = "Resources/Default/DefaultScript.cs";

    EditorAssetsLibrary EditorAssets::s_Library;

    void EditorAssets::Load()
    {
        s_Library.UnassignedTexture = Importer::Get().Import<Texture>(UnassignedTexture);

        s_Library.PlayIcon = Importer::Get().Import<Texture>(PlayIcon);
        s_Library.StopIcon = Importer::Get().Import<Texture>(StopIcon);
        s_Library.PauseIcon = Importer::Get().Import<Texture>(PauseIcon);

        s_Library.FolderIcon = Importer::Get().Import<Texture>(FolderIcon);
        s_Library.FileIcon = Importer::Get().Import<Texture>(FileIcon);

        s_Library.ArrowPointerIcon = Importer::Get().Import<Texture>(ArrowPointerIcon);
        s_Library.ArrowsIcon = Importer::Get().Import<Texture>(ArrowsIcon);
        s_Library.RotateIcon = Importer::Get().Import<Texture>(RotateIcon);
        s_Library.MaximizeIcon = Importer::Get().Import<Texture>(MaximizeIcon);
        s_Library.GlobeIcon = Importer::Get().Import<Texture>(GlobeIcon);
        s_Library.SearchIcon = Importer::Get().Import<Texture>(SearchIcon);

        s_Library.ConsoleInfo = Importer::Get().Import<Texture>(ConsoleInfo);
        s_Library.ConsoleWarn = Importer::Get().Import<Texture>(ConsoleWarn);
        s_Library.ConsoleError = Importer::Get().Import<Texture>(ConsoleError);

        s_Library.AlignLeft = Importer::Get().Import<Texture>(AlignLeft);
        s_Library.AlignCenter = Importer::Get().Import<Texture>(AlignCenter);
        s_Library.AlignRight = Importer::Get().Import<Texture>(AlignRight);

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Library.Test = std::static_pointer_cast<Font>(font)->GetAtlasTexture();
    }

    void EditorAssets::Unload()
    {
        s_Library.UnassignedTexture = nullptr;
        s_Library.PlayIcon = nullptr;
        s_Library.StopIcon = nullptr;
        s_Library.PauseIcon = nullptr;
        s_Library.FolderIcon = nullptr;
        s_Library.FileIcon = nullptr;
        s_Library.ArrowPointerIcon = nullptr;
        s_Library.ArrowsIcon = nullptr;
        s_Library.RotateIcon = nullptr;
        s_Library.MaximizeIcon = nullptr;
        s_Library.GlobeIcon = nullptr;
        s_Library.SearchIcon = nullptr;
        s_Library.ConsoleInfo = nullptr;
        s_Library.ConsoleWarn = nullptr;
        s_Library.ConsoleError = nullptr;
        s_Library.AlignLeft = nullptr;
        s_Library.AlignCenter = nullptr;
        s_Library.AlignRight = nullptr;
    }
} // namespace Crowny