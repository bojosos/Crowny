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

    const String EditorAssets::DefaultScriptPath = "Resources/Default/DefaultScript.cs";

    EditorAssetsLibrary EditorAssets::s_Library;

    void EditorAssets::Load()
    {
        Ref<ImportOptions> importOptions = CreateRef<TextureImportOptions>();

        s_Library.UnassignedTexture = Importer::Get().Import<Texture>(UnassignedTexture, importOptions);
        s_Library.PlayIcon = Importer::Get().Import<Texture>(PlayIcon, importOptions);
        s_Library.StopIcon = Importer::Get().Import<Texture>(StopIcon, importOptions);
        s_Library.PauseIcon = Importer::Get().Import<Texture>(PauseIcon, importOptions);
        s_Library.FolderIcon = Importer::Get().Import<Texture>(FolderIcon, importOptions);
        s_Library.FileIcon = Importer::Get().Import<Texture>(FileIcon, importOptions);
		s_Library.ArrowPointerIcon = Importer::Get().Import<Texture>(ArrowPointerIcon, importOptions);
        s_Library.ArrowsIcon = Importer::Get().Import<Texture>(ArrowsIcon, importOptions);
        s_Library.RotateIcon = Importer::Get().Import<Texture>(RotateIcon, importOptions);
        s_Library.MaximizeIcon = Importer::Get().Import<Texture>(MaximizeIcon, importOptions);
		s_Library.GlobeIcon = Importer::Get().Import<Texture>(GlobeIcon, importOptions);
        s_Library.SearchIcon = Importer::Get().Import<Texture>(SearchIcon, importOptions);
    }

    void EditorAssets::Unload()
    {
        s_Library.UnassignedTexture = nullptr;
        s_Library.PlayIcon = nullptr;
        s_Library.StopIcon = nullptr;
        s_Library.PauseIcon = nullptr;
        s_Library.FolderIcon = nullptr;
        s_Library.FileIcon = nullptr;
    }
} // namespace Crowny