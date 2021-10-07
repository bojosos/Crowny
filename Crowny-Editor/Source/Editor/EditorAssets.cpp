#include "cwepch.h"

#include "Editor/EditorAssets.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    const String EditorAssets::UnassignedTexture = "/Textures/Unassigned.png";
    const String EditorAssets::PlayIcon = "/Icons/Play.png";
    const String EditorAssets::PauseIcon = "/Icons/Pause.png";
    const String EditorAssets::StopIcon = "/Icons/Stop.png";
    const String EditorAssets::FileIcon = "/Icons/File.png";
    const String EditorAssets::FolderIcon = "/Icons/Folder.png";

    const String EditorAssets::DefaultScriptPath = "Resources/Default/DefaultScript.cs";

    EditorAssetsLibrary EditorAssets::s_Library;

    void EditorAssets::Load()
    {
        Ref<ImportOptions> importOptions = CreateRef<TextureImportOptions>();
        UUID uuid;
        s_Library.UnassignedTexture = Importer::Get().Import<Texture>(UnassignedTexture, importOptions, uuid);
        s_Library.PlayIcon = Importer::Get().Import<Texture>(PlayIcon, importOptions, uuid);
        s_Library.StopIcon = Importer::Get().Import<Texture>(StopIcon, importOptions, uuid);
        s_Library.PauseIcon = Importer::Get().Import<Texture>(PauseIcon, importOptions, uuid);
        s_Library.FolderIcon = Importer::Get().Import<Texture>(FolderIcon, importOptions, uuid);
        s_Library.FileIcon = Importer::Get().Import<Texture>(FileIcon, importOptions, uuid);
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