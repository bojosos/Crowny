#include "cwpch.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Renderer/TextureManager.h"

namespace Crowny
{

	void Initializer::Init()
	{
		// Most of these should be in the editor
		VirtualFileSystem::Init();
		VirtualFileSystem::Get()->Mount("Shaders", "Resources/Shaders");
		VirtualFileSystem::Get()->Mount("Textures", "Resources/Textures");
		VirtualFileSystem::Get()->Mount("Fonts", "Resources/Fonts");
		Renderer::Init();
		Random::Init();
		FontManager::Add(CreateRef<Font>("Roboto Thin", "/Fonts/" + DEFAULT_FONT_FILENAME, 64));
		Textures::LoadDefault();
	}

	void Initializer::Shutdown()
	{
		Renderer::Shutdown();
		SceneManager::Shutdown();
		VirtualFileSystem::Shutdown();
	}

}