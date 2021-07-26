#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	const std::string EditorAssets::UnassignedTexture = "/Textures/Unassigned.png";
	const std::string EditorAssets::PlayIcon = "/Icons/Play.png";
	const std::string EditorAssets::PauseIcon = "/Icons/Pause.png";
	const std::string EditorAssets::StopIcon = "/Icons/Stop.png";

    EditorAssetsLibrary EditorAssets::s_Library;

	void EditorAssets::Load()
	{
		//AssetManifest editorAssets = AssetManager::ImportManifest("Editor");
		// s_Library.UnassignedTexture = Texture2D::Create(UnassignedTexture);// editorAssets.LoadTexture(UnassignedTexture);
		// s_Library.PlayIcon = Texture2D::Create(PlayIcon);
		// s_Library.PauseIcon = Texture2D::Create(PauseIcon);
		// s_Library.StopIcon = Texture2D::Create(StopIcon);
	}
}