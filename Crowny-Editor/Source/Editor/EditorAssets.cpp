#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Crowny/Renderer/Texture.h"

#include "Crowny/../../Dependencies/stb_image/stb_image.h"

namespace Crowny
{
	const std::string EditorAssets::UnassignedTexture = "/Textures/Unassigned.png";
	const std::string EditorAssets::PlayIcon = "/Icons/Play.png";
	const std::string EditorAssets::PauseIcon = "/Icons/Pause.png";
	const std::string EditorAssets::StopIcon = "/Icons/Stop.png";
	const std::string EditorAssets::FileIcon = "/Icons/File.png";
	const std::string EditorAssets::FolderIcon = "/Icons/Folder.png";

	const std::string EditorAssets::DefaultScriptPath = "Resources/Default/DefaultScript.cs";
	
    EditorAssetsLibrary EditorAssets::s_Library;

	void LoadTexture(const std::string& filepath, Ref<Texture>& texture)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);

		auto [loaded, size] = VirtualFileSystem::Get()->ReadFile(filepath);
		
		auto* data = stbi_load_from_memory(loaded, size, &width, &height, &channels, 0);
		TextureParameters params;
		params.Width = width;
		params.Height = height;
		texture = Texture::Create(params);
		PixelData* pd;
		if (channels == 1)
		{
			pd = new PixelData(width, height, 1, TextureFormat::R8);
			params.Format = TextureFormat::R8;
		} else if (channels == 3)
		{
			pd = new PixelData(width, height, 1, TextureFormat::RGB8);
			params.Format = TextureFormat::RGB8;
		}
		else
			pd  = new PixelData(width, height, 1, TextureFormat::RGBA8);
		pd->SetBuffer(data);
		texture->WriteData(*pd);
		pd->SetBuffer(nullptr);
		delete pd;
	}
	
	void EditorAssets::Load()
	{
		LoadTexture(UnassignedTexture, s_Library.UnassignedTexture);
		LoadTexture(PlayIcon, s_Library.PlayIcon);
		LoadTexture(StopIcon, s_Library.StopIcon);
		LoadTexture(PauseIcon, s_Library.PauseIcon);
		LoadTexture(FolderIcon, s_Library.FolderIcon);
		LoadTexture(FileIcon, s_Library.FileIcon);
	}
}