#include "cwpch.h"

#include "Crowny/Application/Initializer.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{

	void Initializer::Init()
	{
		Renderer::Init();
		Random::Init();
		FontManager::Add(CreateRef<Font>("default", DEFAULT_FONT_PATH, 64));
	}

	void Initializer::Shutdown()
	{
		Renderer::Shutdown();
		SceneManager::Shutdown();
	}

}