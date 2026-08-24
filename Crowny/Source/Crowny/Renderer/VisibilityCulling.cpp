#include "cwpch.h"

#include "Crowny/Renderer/VisibilityCulling.h"

namespace Crowny
{
    namespace
    {
        glm::vec4 NormalizePlane(const glm::vec4& plane)
        {
            const float length = glm::length(glm::vec3(plane));
            return length > 0.000001f ? plane / length : glm::vec4(0.0f);
        }
    } // namespace

    VisibilityFrustum VisibilityFrustum::FromViewProjection(const glm::mat4& viewProjection, bool zeroToOneDepth)
    {
        const glm::mat4 rows = glm::transpose(viewProjection);
        VisibilityFrustum frustum;
        frustum.Planes[0] = NormalizePlane(rows[3] + rows[0]);
        frustum.Planes[1] = NormalizePlane(rows[3] - rows[0]);
        frustum.Planes[2] = NormalizePlane(rows[3] + rows[1]);
        frustum.Planes[3] = NormalizePlane(rows[3] - rows[1]);
        frustum.Planes[4] = NormalizePlane(zeroToOneDepth ? rows[2] : rows[3] + rows[2]);
        frustum.Planes[5] = NormalizePlane(rows[3] - rows[2]);
        return frustum;
    }

    bool VisibilityFrustum::IntersectsSphere(const glm::vec3& center, float radius) const
    {
        radius = std::max(radius, 0.0f);
        for (const glm::vec4& plane : Planes)
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
                return false;
        return true;
    }

    void VisibilityCullingStats::Add(VisibilityCullReason reason)
    {
        switch (reason)
        {
        case VisibilityCullReason::Visible:
            Visible++;
            break;
        case VisibilityCullReason::Hidden:
            Hidden++;
            break;
        case VisibilityCullReason::Layer:
            Layer++;
            break;
        case VisibilityCullReason::Frustum:
            Frustum++;
            break;
        case VisibilityCullReason::ProjectedSize:
            ProjectedSize++;
            break;
        case VisibilityCullReason::NormalCone:
            NormalCone++;
            break;
        case VisibilityCullReason::Occluded:
            Occluded++;
            break;
        }
    }

    float VisibilityCulling::ProjectedSphereDiameter(float radius, float viewDepth, float projectionYScale,
                                                      float viewportHeight)
    {
        if (radius <= 0.0f || viewDepth <= 0.000001f || projectionYScale <= 0.0f || viewportHeight <= 0.0f)
            return 0.0f;
        return radius * projectionYScale * viewportHeight / viewDepth;
    }

    uint32_t VisibilityCulling::SelectLod(const MeshGpuGeometry& geometry, float viewDepth, float projectionYScale,
                                          float viewportHeight, float maximumErrorPixels, float lodBias)
    {
        if (geometry.Lods.empty())
            return 0;
        maximumErrorPixels = std::max(maximumErrorPixels, 0.0f);
        const float biasScale = std::exp2(std::clamp(lodBias, -8.0f, 8.0f));
        const float projectionScale = projectionYScale * viewportHeight * 0.5f;
        uint32_t selected = 0;
        for (uint32_t lodIndex = 1; lodIndex < geometry.Lods.size(); lodIndex++)
        {
            const float projectedError = geometry.Lods[lodIndex].Error * projectionScale /
                                         std::max(viewDepth * biasScale, 0.000001f);
            if (projectedError > maximumErrorPixels)
                break;
            selected = lodIndex;
        }
        return selected;
    }

    bool VisibilityCulling::IsMeshletBackfacing(const glm::vec3& sphereCenter, float sphereRadius,
                                                 const glm::vec3& coneAxis, float coneCutoff,
                                                 const glm::vec3& cameraPosition)
    {
        if (coneCutoff >= 1.0f)
            return false;
        const glm::vec3 toCenter = sphereCenter - cameraPosition;
        const float distance = glm::length(toCenter);
        const glm::vec3 axis = glm::dot(coneAxis, coneAxis) > 0.000001f ? glm::normalize(coneAxis) : glm::vec3(0.0f);
        return glm::dot(toCenter, axis) >= coneCutoff * distance + std::max(sphereRadius, 0.0f);
    }

    float VisibilityCulling::ReduceReverseZ(const float* depths, uint32_t count)
    {
        float farthestDepth = 1.0f;
        for (uint32_t index = 0; depths != nullptr && index < count; index++)
            farthestDepth = std::min(farthestDepth, std::clamp(depths[index], 0.0f, 1.0f));
        return farthestDepth;
    }

    bool VisibilityCulling::IsOccludedReverseZ(float objectNearestDepth, float hiZFarthestDepth, float bias)
    {
        objectNearestDepth = std::clamp(objectNearestDepth, 0.0f, 1.0f);
        hiZFarthestDepth = std::clamp(hiZFarthestDepth, 0.0f, 1.0f);
        return objectNearestDepth <= hiZFarthestDepth - std::max(bias, 0.0f);
    }
} // namespace Crowny
