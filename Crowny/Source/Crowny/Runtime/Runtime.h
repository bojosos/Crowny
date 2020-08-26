#pragma once

#include "Crowny/SceneManagement/Scene.h"

namespace Crowny
{
	class Runtime
	{
	public:
		Runtime(Scene* scene);
	private:
		void Init();
	};
}