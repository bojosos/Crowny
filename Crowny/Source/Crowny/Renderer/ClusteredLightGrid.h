#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    struct ClusteredLightGridDesc
    {
        uint32_t ViewportWidth = 1;
        uint32_t ViewportHeight = 1;
        uint32_t TileSize = 16;
        uint32_t DepthSlices = 24;
        uint32_t MaxLightsPerCluster = 64;
        uint32_t MaxDirectionalLights = 8;
        float NearPlane = 0.05f;
        float FarPlane = 1000.0f;
        RenderLayerMask VisibilityMask = RenderLayerMask::All();
    };

    struct ClusteredLightCell
    {
        uint32_t Offset = 0;
        uint32_t Count = 0;
    };

    struct ClusteredLightGrid
    {
        glm::uvec3 Dimensions = glm::uvec3(1u);
        Vector<ClusteredLightCell> Cells;
        Vector<uint32_t> LightIndices;
        Vector<uint32_t> DirectionalLightIndices;
        uint32_t OverflowCount = 0;

        void Clear()
        {
            Dimensions = glm::uvec3(1u);
            Cells.clear();
            LightIndices.clear();
            DirectionalLightIndices.clear();
            OverflowCount = 0;
        }
    };

    // Deterministic CPU reference and compatibility fallback. Vulkan uses the
    // same projection and logarithmic-slice equations in compute.
    class ClusteredLightBuilder
    {
    public:
        static void Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection, const RenderLightData* lights,
                          uint32_t lightCount, ClusteredLightGrid& output);
        static ClusteredLightGridDesc ResolveDesc(const RenderPipelineSettings& settings, uint32_t viewportWidth, uint32_t viewportHeight,
                                                  float nearPlane = 0.05f, float farPlane = 1000.0f,
                                                  RenderLayerMask visibilityMask = RenderLayerMask::All());
        static glm::uvec3 GetDimensions(const ClusteredLightGridDesc& desc);
        static uint64_t GetClusterCount(const ClusteredLightGridDesc& desc);
        static uint32_t DepthToSlice(float depth, float nearPlane, float farPlane, uint32_t depthSlices);
        static uint32_t Flatten(uint32_t x, uint32_t y, uint32_t z, const glm::uvec3& dimensions)
        {
            return x + dimensions.x * (y + dimensions.y * z);
        }
    };
} // namespace Crowny
