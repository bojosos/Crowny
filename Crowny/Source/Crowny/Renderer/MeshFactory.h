#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class MeshFactory
    {
    public:
        static Ref<Mesh> CreatePlane(float width, float height, const glm::vec3& normal,
                                     const Ref<MaterialInstance>& material);
        static Ref<Mesh> CreateCube(float size, const Ref<MaterialInstance>& material);
        static Ref<Mesh> CreateSphere(float xSegements = 64, float ySegments = 64);
    };
} // namespace Crowny