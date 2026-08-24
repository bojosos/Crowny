#pragma once

#include "Crowny/Renderer/DirectionalShadowCascades.h"
#include "Crowny/Renderer/LocalShadowViews.h"
#include "Crowny/Renderer/ShadowAtlas.h"

namespace Crowny
{
    enum class GpuShadowFlags : uint32_t
    {
        None = 0,
        Valid = 1 << 0,
        Cube = 1 << 1,
        Soft = 1 << 2
    };

    struct alignas(16) GpuShadowLightData
    {
        uint32_t ViewOffset = 0;
        uint32_t ViewCount = 0;
        uint32_t Type = 0;
        uint32_t Flags = 0;
    };

    struct alignas(16) GpuShadowViewData
    {
        glm::mat4 ViewProjection = glm::mat4(1.0f);
        // UV scale and bias for a 2D atlas. Point-light records leave this at
        // identity and use Metadata.x as the cube-array layer.
        glm::vec4 AtlasScaleBias = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
        // x/y are camera split depths, z/w are depth and normal bias.
        glm::vec4 SplitDepthBias = glm::vec4(0.0f);
        glm::uvec4 Metadata = glm::uvec4(0u);
    };

    static_assert(sizeof(GpuShadowLightData) == 16);
    static_assert(sizeof(GpuShadowViewData) == 112);

    class ShadowGpuDataBuilder
    {
    public:
        static GpuShadowViewData BuildSpot(const LocalShadowView& view, const ShadowAtlasAllocation& allocation,
                                           uint32_t atlasSize, const LightShadowSettings& settings);
        static void BuildPoint(const std::array<LocalShadowView, 6>& views, uint32_t cubeLayer,
                               const LightShadowSettings& settings, Vector<GpuShadowViewData>& output);
        static void BuildDirectional(const DirectionalShadowCascade* cascades,
                                     const ShadowAtlasAllocation* allocations, uint32_t cascadeCount,
                                     uint32_t atlasSize, const LightShadowSettings& settings,
                                     Vector<GpuShadowViewData>& output);
        static void BuildDirectionalArray(const DirectionalShadowCascade* cascades, uint32_t cascadeCount,
                                          const LightShadowSettings& settings, Vector<GpuShadowViewData>& output);
        static GpuShadowLightData BuildLightRecord(uint32_t viewOffset, uint32_t viewCount, LightType type,
                                                   const LightShadowSettings& settings);
    };
} // namespace Crowny
