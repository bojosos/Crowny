#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Crowny
{
	
	Ref<Mesh> CreatePlane(float width, float height, const glm::vec3& normal, const Ref<MaterialInstance>& material);
	Ref<Mesh> CreateCube(float size, const Ref<MaterialInstance>& material);
}