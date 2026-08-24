#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    struct VisibilityFrustum
    {
        std::array<glm::vec4, 6> Planes{};

        static VisibilityFrustum FromViewProjection(const glm::mat4& viewProjection, bool zeroToOneDepth = true);
        bool IntersectsSphere(const glm::vec3& center, float radius) const;
    };

    enum class VisibilityCullReason : uint8_t
    {
        Visible,
        Hidden,
        Layer,
        Frustum,
        ProjectedSize,
        NormalCone,
        Occluded
    };

    struct VisibilityCullingStats
    {
        uint32_t Visible = 0;
        uint32_t Hidden = 0;
        uint32_t Layer = 0;
        uint32_t Frustum = 0;
        uint32_t ProjectedSize = 0;
        uint32_t NormalCone = 0;
        uint32_t Occluded = 0;

        void Add(VisibilityCullReason reason);
    };

    class VisibilityCulling
    {
    public:
        static float ProjectedSphereDiameter(float radius, float viewDepth, float projectionYScale, float viewportHeight);
        static uint32_t SelectLod(const MeshGpuGeometry& geometry, float viewDepth, float projectionYScale,
                                  float viewportHeight, float maximumErrorPixels, float lodBias = 0.0f);
        static bool IsMeshletBackfacing(const glm::vec3& sphereCenter, float sphereRadius, const glm::vec3& coneAxis,
                                        float coneCutoff, const glm::vec3& cameraPosition);
        static float ReduceReverseZ(const float* depths, uint32_t count);
        static bool IsOccludedReverseZ(float objectNearestDepth, float hiZFarthestDepth, float bias = 0.0005f);
    };
} // namespace Crowny
