#pragma once

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
	class Runtime
	{
	public:
		Runtime();

	private:
		void OnSceneChanged(const Ref<Scene>& scene);
		void OnStartup();
		void OnShutdown();

		Ref<Scene> m_Scene = nullptr;
		Ref<Scene> m_OpenScene = nullptr;
	};
}