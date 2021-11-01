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