#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	const std::string EditorAssets::UnassignedTexture = "Textures/Unassigned.png";

    EditorAssetsLibrary EditorAssets::s_Library;

	void EditorAssets::Load()
	{
		//AssetManifest editorAssets = AssetManager::ImportManifest("Editor");
		s_Library.UnassignedTexture = Texture2D::Create(UnassignedTexture);// editorAssets.LoadTexture(UnassignedTexture);
	}
}