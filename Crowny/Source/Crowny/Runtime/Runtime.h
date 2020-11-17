#pragma once

#include "Crowny/SceneManagement/Scene.h"

namespace Crowny
{
	class Runtime
	{
	public:
		Runtime();

	private:
		void OnSceneChanged(Scene* scene);
		void OnStartup();
		void OnShutdown();

		Scene* m_Scene = nullptr;
		Scene* m_OpenScene = nullptr;
	};
}