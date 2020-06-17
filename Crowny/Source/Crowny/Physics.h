#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

	class Physics
	{
	public:
		static void VoxelRaycast(const glm::vec3& location, const glm::vec3& direction, float length, bool button);
	};
	
}