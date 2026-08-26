#include "cwpch.h"

#include "Crowny/Renderer/DirectionalShadowCascades.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <array>
#include <span>

namespace Crowny
{
    namespace
    {
        void WriteSplits(float cameraNear, float shadowDistance, uint32_t cascadeCount, float splitLambda,
                         std::span<float> output)
        {
            CW_ENGINE_ASSERT(output.size() >= cascadeCount + 1u);
            output[0] = cameraNear;
            for (uint32_t cascade = 1; cascade < cascadeCount; cascade++)
            {
                const float fraction = static_cast<float>(cascade) / cascadeCount;
                const float logarithmic = cameraNear * std::pow(shadowDistance / cameraNear, fraction);
                const float uniform = cameraNear + (shadowDistance - cameraNear) * fraction;
                output[cascade] = glm::mix(uniform, logarithmic, splitLambda);
            }
            output[cascadeCount] = shadowDistance;
        }
    } // namespace

    void DirectionalShadowCascadeBuilder::CalculateSplits(float cameraNear, float shadowDistance, uint32_t cascadeCount,
                                                            float splitLambda, Vector<float>& output)
    {
        cameraNear = std::max(cameraNear, 0.0001f);
        shadowDistance = std::max(shadowDistance, cameraNear + 0.0001f);
        cascadeCount = std::clamp(cascadeCount, 1u, 4u);
        splitLambda = std::clamp(splitLambda, 0.0f, 1.0f);
        output.resize(cascadeCount + 1u);
        WriteSplits(cameraNear, shadowDistance, cascadeCount, splitLambda, output);
    }

    void DirectionalShadowCascadeBuilder::Build(const glm::mat4& cameraWorld, float verticalFovRadians, float aspectRatio,
                                                  float cameraNear, const glm::vec3& lightDirection,
                                                  const DirectionalShadowCascadeSettings& settings,
                                                  Vector<DirectionalShadowCascade>& output)
    {
        const uint32_t cascadeCount = std::clamp(settings.CascadeCount, 1u, 4u);
        const uint32_t resolution = std::max(settings.Resolution, 1u);
        verticalFovRadians = std::clamp(verticalFovRadians, glm::radians(1.0f), glm::radians(179.0f));
        aspectRatio = std::max(aspectRatio, 0.001f);

        std::array<float, 5> splits;
        cameraNear = std::max(cameraNear, 0.0001f);
        const float shadowDistance = std::max(settings.ShadowDistance, cameraNear + 0.0001f);
        const float splitLambda = std::clamp(settings.SplitLambda, 0.0f, 1.0f);
        WriteSplits(cameraNear, shadowDistance, cascadeCount, splitLambda, splits);
        output.resize(cascadeCount);

        const glm::vec3 direction = glm::dot(lightDirection, lightDirection) > 0.000001f
                                      ? glm::normalize(lightDirection)
                                      : glm::vec3(0.0f, -1.0f, 0.0f);
        const glm::vec3 referenceUp = std::abs(direction.y) > 0.95f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                                    : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 lightRight = glm::normalize(glm::cross(direction, referenceUp));
        const glm::vec3 lightUp = glm::normalize(glm::cross(lightRight, direction));
        const float tanHalfFov = std::tan(verticalFovRadians * 0.5f);

        for (uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; cascadeIndex++)
        {
            const float nearSplit = splits[cascadeIndex];
            const float farSplit = splits[cascadeIndex + 1u];
            const float nearY = nearSplit * tanHalfFov;
            const float nearX = nearY * aspectRatio;
            const float farY = farSplit * tanHalfFov;
            const float farX = farY * aspectRatio;

            const glm::vec3 viewCorners[8] = {
                { -nearX, -nearY, -nearSplit }, { nearX, -nearY, -nearSplit },
                { nearX, nearY, -nearSplit },   { -nearX, nearY, -nearSplit },
                { -farX, -farY, -farSplit },   { farX, -farY, -farSplit },
                { farX, farY, -farSplit },     { -farX, farY, -farSplit },
            };

            glm::vec3 center(0.0f);
            std::array<glm::vec3, 8> worldCorners;
            for (uint32_t corner = 0; corner < worldCorners.size(); corner++)
            {
                worldCorners[corner] = glm::vec3(cameraWorld * glm::vec4(viewCorners[corner], 1.0f));
                center += worldCorners[corner];
            }
            center /= static_cast<float>(worldCorners.size());

            float radius = 0.0f;
            for (const glm::vec3& corner : worldCorners)
                radius = std::max(radius, glm::distance(corner, center));
            // Quantizing the extent prevents small camera rotations from
            // continuously changing the shadow projection scale.
            radius = std::ceil(radius * 16.0f) / 16.0f;
            const float texelWorldSize = 2.0f * radius / resolution;

            const float lightX = glm::dot(center, lightRight);
            const float lightY = glm::dot(center, lightUp);
            const float snappedX = std::round(lightX / texelWorldSize) * texelWorldSize;
            const float snappedY = std::round(lightY / texelWorldSize) * texelWorldSize;
            center += lightRight * (snappedX - lightX) + lightUp * (snappedY - lightY);

            const float depthPadding = std::max(settings.DepthPadding, 0.0f);
            const float eyeDistance = radius + depthPadding;
            DirectionalShadowCascade& cascade = output[cascadeIndex];
            cascade.View = glm::lookAtRH(center - direction * eyeDistance, center, lightUp);
            cascade.Projection = glm::orthoRH_ZO(-radius, radius, -radius, radius, 0.0f,
                                                  2.0f * eyeDistance);
            cascade.ViewProjection = cascade.Projection * cascade.View;
            cascade.BoundingSphere = { center, radius };
            cascade.NearSplit = nearSplit;
            cascade.FarSplit = farSplit;
            cascade.TexelWorldSize = texelWorldSize;
        }
    }
} // namespace Crowny
