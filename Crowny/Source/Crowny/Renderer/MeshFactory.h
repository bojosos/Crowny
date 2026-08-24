#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class MeshFactory
    {
    public:
        static Ref<MeshData> CreatePlaneData(float width = 1.0f, float height = 1.0f,
                                             const glm::vec3& normal = glm::vec3(0.0f, 1.0f, 0.0f), uint32_t subdivisionsX = 1,
                                             uint32_t subdivisionsY = 1);
        static Ref<MeshData> CreateBoxData(const glm::vec3& dimensions = glm::vec3(1.0f));
        static Ref<MeshData> CreateCubeData(float size = 1.0f);
        static Ref<MeshData> CreateSphereData(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);
        static Ref<MeshData> CreateCylinderData(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32, bool capped = true);
        static Ref<MeshData> CreateConeData(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32, bool capped = true);
        static Ref<MeshData> CreateCapsuleData(float radius = 0.5f, float height = 2.0f, uint32_t segments = 32,
                                               uint32_t hemisphereRings = 8);

        static Ref<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f,
                                     const glm::vec3& normal = glm::vec3(0.0f, 1.0f, 0.0f), uint32_t subdivisionsX = 1,
                                     uint32_t subdivisionsY = 1, MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateBox(const glm::vec3& dimensions = glm::vec3(1.0f), MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateCube(float size = 1.0f, MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateSphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16,
                                      MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateCylinder(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32, bool capped = true,
                                        MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateCone(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32, bool capped = true,
                                    MeshUsageFlags usage = MeshUsage::Static);
        static Ref<Mesh> CreateCapsule(float radius = 0.5f, float height = 2.0f, uint32_t segments = 32, uint32_t hemisphereRings = 8,
                                       MeshUsageFlags usage = MeshUsage::Static);
    };
} // namespace Crowny
