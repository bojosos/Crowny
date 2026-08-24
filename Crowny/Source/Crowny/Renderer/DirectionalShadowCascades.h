#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{
    struct DirectionalShadowCascadeSettings
    {
        uint32_t CascadeCount = 3;
        uint32_t Resolution = 2048;
        float ShadowDistance = 150.0f;
        float SplitLambda = 0.65f;
        float DepthPadding = 50.0f;
    };

    struct DirectionalShadowCascade
    {
        glm::mat4 View = glm::mat4(1.0f);
        glm::mat4 Projection = glm::mat4(1.0f);
        glm::mat4 ViewProjection = glm::mat4(1.0f);
        glm::vec4 BoundingSphere = glm::vec4(0.0f);
        float NearSplit = 0.0f;
        float FarSplit = 0.0f;
        float TexelWorldSize = 0.0f;
    };

    class DirectionalShadowCascadeBuilder
    {
    public:
        static void Build(const glm::mat4& cameraWorld, float verticalFovRadians, float aspectRatio, float cameraNear,
                          const glm::vec3& lightDirection, const DirectionalShadowCascadeSettings& settings,
                          Vector<DirectionalShadowCascade>& output);

        static void CalculateSplits(float cameraNear, float shadowDistance, uint32_t cascadeCount, float splitLambda,
                                    Vector<float>& output);
    };
} // namespace Crowny
