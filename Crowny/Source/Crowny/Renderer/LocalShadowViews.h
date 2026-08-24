#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    struct LocalShadowView
    {
        glm::mat4 View = glm::mat4(1.0f);
        glm::mat4 Projection = glm::mat4(1.0f);
        glm::mat4 ViewProjection = glm::mat4(1.0f);
        glm::vec3 Direction = glm::vec3(0.0f, 0.0f, -1.0f);
        uint32_t Face = 0;
        float NearPlane = 0.05f;
        float FarPlane = 10.0f;
    };

    class LocalShadowViewBuilder
    {
    public:
        static LocalShadowView BuildSpot(const RenderLightData& light, const LightShadowSettings& settings);
        static void BuildPoint(const RenderLightData& light, const LightShadowSettings& settings,
                               std::array<LocalShadowView, 6>& output);

    private:
        static glm::vec3 SelectUp(const glm::vec3& direction);
    };
} // namespace Crowny
