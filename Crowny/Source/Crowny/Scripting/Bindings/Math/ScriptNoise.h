#pragma once

#include "Crowny/Ecs/Components.h"

#include <glm/glm.hpp>

namespace Crowny
{
	
	class ScriptNoise
	{
	public:
		static void InitRuntimeFunctions();

	private:
		static float Internal_PerlinNoise(float x, float y);
   
	};
}
