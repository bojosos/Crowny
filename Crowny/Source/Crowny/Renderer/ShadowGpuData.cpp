#include "cwpch.h"

#include "Crowny/Renderer/ShadowGpuData.h"

namespace Crowny
{
    namespace
    {
        glm::vec4 AtlasTransform(const ShadowAtlasAllocation& allocation, uint32_t atlasSize)
        {
            const float inverseSize = 1.0f / static_cast<float>(std::max(atlasSize, 1u));
            return { allocation.Size * inverseSize, allocation.Size * inverseSize,
                     allocation.X * inverseSize, allocation.Y * inverseSize };
        }

        uint32_t ShadowFlags(LightType type, const LightShadowSettings& settings)
        {
            if (settings.Mode == LightShadowMode::Disabled)
                return 0;
            uint32_t flags = static_cast<uint32_t>(GpuShadowFlags::Valid);
            if (type == LightType::Point)
                flags |= static_cast<uint32_t>(GpuShadowFlags::Cube);
            if (settings.Mode == LightShadowMode::Soft)
                flags |= static_cast<uint32_t>(GpuShadowFlags::Soft);
            return flags;
        }
    } // namespace

    GpuShadowViewData ShadowGpuDataBuilder::BuildSpot(const LocalShadowView& view,
                                                       const ShadowAtlasAllocation& allocation, uint32_t atlasSize,
                                                       const LightShadowSettings& settings)
    {
        GpuShadowViewData output;
        output.ViewProjection = view.ViewProjection;
        output.AtlasScaleBias = AtlasTransform(allocation, atlasSize);
        output.SplitDepthBias = { view.NearPlane, view.FarPlane, std::max(settings.Bias, 0.0f),
                                  std::max(settings.NormalBias, 0.0f) };
        return output;
    }

    void ShadowGpuDataBuilder::BuildPoint(const std::array<LocalShadowView, 6>& views, uint32_t cubeLayer,
                                          const LightShadowSettings& settings, Vector<GpuShadowViewData>& output)
    {
        output.reserve(output.size() + views.size());
        for (const LocalShadowView& view : views)
        {
            GpuShadowViewData& record = output.emplace_back();
            record.ViewProjection = view.ViewProjection;
            record.SplitDepthBias = { view.NearPlane, view.FarPlane, std::max(settings.Bias, 0.0f),
                                      std::max(settings.NormalBias, 0.0f) };
            record.Metadata = { cubeLayer, view.Face, 0u, 0u };
        }
    }

    void ShadowGpuDataBuilder::BuildDirectional(const DirectionalShadowCascade* cascades,
                                                const ShadowAtlasAllocation* allocations, uint32_t cascadeCount,
                                                uint32_t atlasSize, const LightShadowSettings& settings,
                                                Vector<GpuShadowViewData>& output)
    {
        output.reserve(output.size() + cascadeCount);
        for (uint32_t index = 0; cascades != nullptr && allocations != nullptr && index < cascadeCount; index++)
        {
            GpuShadowViewData& record = output.emplace_back();
            record.ViewProjection = cascades[index].ViewProjection;
            record.AtlasScaleBias = AtlasTransform(allocations[index], atlasSize);
            record.SplitDepthBias = { cascades[index].NearSplit, cascades[index].FarSplit,
                                      std::max(settings.Bias, 0.0f), std::max(settings.NormalBias, 0.0f) };
            record.Metadata.y = index;
        }
    }

    void ShadowGpuDataBuilder::BuildDirectionalArray(const DirectionalShadowCascade* cascades, uint32_t cascadeCount,
                                                     const LightShadowSettings& settings,
                                                     Vector<GpuShadowViewData>& output)
    {
        output.reserve(output.size() + cascadeCount);
        for (uint32_t index = 0; cascades != nullptr && index < cascadeCount; index++)
        {
            GpuShadowViewData& record = output.emplace_back();
            record.ViewProjection = cascades[index].ViewProjection;
            record.SplitDepthBias = { cascades[index].NearSplit, cascades[index].FarSplit,
                                      std::max(settings.Bias, 0.0f), std::max(settings.NormalBias, 0.0f) };
            record.Metadata = { index, index, 0u, 0u };
        }
    }

    GpuShadowLightData ShadowGpuDataBuilder::BuildLightRecord(uint32_t viewOffset, uint32_t viewCount, LightType type,
                                                              const LightShadowSettings& settings)
    {
        return { viewOffset, viewCount, static_cast<uint32_t>(type), ShadowFlags(type, settings) };
    }
} // namespace Crowny
