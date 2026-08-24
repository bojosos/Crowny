#include "cwpch.h"

#include "Crowny/Renderer/LocalShadowViews.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Crowny
{
    glm::vec3 LocalShadowViewBuilder::SelectUp(const glm::vec3& direction)
    {
        return std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    LocalShadowView LocalShadowViewBuilder::BuildSpot(const RenderLightData& light, const LightShadowSettings& settings)
    {
        LocalShadowView output;
        const glm::vec3 position = glm::vec3(light.PositionRange);
        const glm::vec3 sourceDirection = glm::vec3(light.DirectionOuterCosine);
        output.Direction = glm::dot(sourceDirection, sourceDirection) > 0.000001f
                             ? glm::normalize(sourceDirection)
                             : glm::vec3(0.0f, 0.0f, -1.0f);
        output.NearPlane = std::clamp(settings.NearPlane, 0.001f, std::max(light.PositionRange.w * 0.5f, 0.001f));
        output.FarPlane = std::max(light.PositionRange.w, output.NearPlane + 0.001f);
        const float outerCosine = std::clamp(light.DirectionOuterCosine.w, -1.0f, 1.0f);
        const float fieldOfView = std::clamp(2.0f * std::acos(outerCosine), glm::radians(1.0f), glm::radians(179.0f));
        output.View = glm::lookAtRH(position, position + output.Direction, SelectUp(output.Direction));
        output.Projection = glm::perspectiveRH_ZO(fieldOfView, 1.0f, output.NearPlane, output.FarPlane);
        output.ViewProjection = output.Projection * output.View;
        return output;
    }

    void LocalShadowViewBuilder::BuildPoint(const RenderLightData& light, const LightShadowSettings& settings,
                                             std::array<LocalShadowView, 6>& output)
    {
        static const std::array<glm::vec3, 6> directions{
            glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
        };
        static const std::array<glm::vec3, 6> upVectors{
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        };

        const glm::vec3 position = glm::vec3(light.PositionRange);
        const float nearPlane = std::clamp(settings.NearPlane, 0.001f, std::max(light.PositionRange.w * 0.5f, 0.001f));
        const float farPlane = std::max(light.PositionRange.w, nearPlane + 0.001f);
        const glm::mat4 projection = glm::perspectiveRH_ZO(glm::half_pi<float>(), 1.0f, nearPlane, farPlane);
        for (uint32_t face = 0; face < output.size(); face++)
        {
            LocalShadowView& view = output[face];
            view.Direction = directions[face];
            view.Face = face;
            view.NearPlane = nearPlane;
            view.FarPlane = farPlane;
            view.View = glm::lookAtRH(position, position + view.Direction, upVectors[face]);
            view.Projection = projection;
            view.ViewProjection = projection * view.View;
        }
    }
} // namespace Crowny
