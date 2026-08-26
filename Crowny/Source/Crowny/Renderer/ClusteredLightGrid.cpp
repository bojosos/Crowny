#include "cwpch.h"

#include "Crowny/Renderer/ClusteredLightGrid.h"

namespace Crowny
{
    namespace
    {
        struct ClusterBounds
        {
            glm::uvec3 Minimum = glm::uvec3(0u);
            glm::uvec3 Maximum = glm::uvec3(0u);
            bool Valid = false;
        };

        ClusterBounds ProjectSphere(const ClusteredLightGridDesc& desc, const glm::uvec3& dimensions, const glm::mat4& view,
                                    const glm::mat4& projection, const glm::vec3& position, float radius)
        {
            const glm::vec3 viewPosition = glm::vec3(view * glm::vec4(position, 1.0f));
            const float centerDepth = -viewPosition.z;
            const float minimumDepth = std::max(desc.NearPlane, centerDepth - radius);
            const float maximumDepth = std::min(desc.FarPlane, centerDepth + radius);
            if (maximumDepth < desc.NearPlane || minimumDepth > desc.FarPlane || minimumDepth > maximumDepth)
                return {};

            ClusterBounds result;
            result.Minimum.z = ClusteredLightBuilder::DepthToSlice(minimumDepth, desc.NearPlane, desc.FarPlane, dimensions.z);
            result.Maximum.z = ClusteredLightBuilder::DepthToSlice(maximumDepth, desc.NearPlane, desc.FarPlane, dimensions.z);

            glm::vec2 minimumNdc(-1.0f);
            glm::vec2 maximumNdc(1.0f);
            if (centerDepth > radius + desc.NearPlane)
            {
                const glm::vec4 clip = projection * glm::vec4(viewPosition, 1.0f);
                if (std::abs(clip.w) > 0.000001f)
                {
                    const glm::vec2 centerNdc = glm::vec2(clip) / clip.w;
                    const float nearestDepth = std::max(centerDepth - radius, desc.NearPlane);
                    const glm::vec2 radiusNdc = glm::abs(glm::vec2(projection[0][0], projection[1][1])) * radius / nearestDepth;
                    const glm::vec2 unclampedMinimum = centerNdc - radiusNdc;
                    const glm::vec2 unclampedMaximum = centerNdc + radiusNdc;
                    if (unclampedMinimum.x > 1.0f || unclampedMinimum.y > 1.0f || unclampedMaximum.x < -1.0f || unclampedMaximum.y < -1.0f)
                        return {};
                    minimumNdc = glm::max(unclampedMinimum, glm::vec2(-1.0f));
                    maximumNdc = glm::min(unclampedMaximum, glm::vec2(1.0f));
                }
            }
            const glm::vec2 minimumPixel = (minimumNdc * 0.5f + 0.5f) * glm::vec2(desc.ViewportWidth, desc.ViewportHeight);
            const glm::vec2 maximumPixel = (maximumNdc * 0.5f + 0.5f) * glm::vec2(desc.ViewportWidth, desc.ViewportHeight);
            result.Minimum.x = std::min(static_cast<uint32_t>(std::max(minimumPixel.x, 0.0f)) / desc.TileSize, dimensions.x - 1u);
            result.Minimum.y = std::min(static_cast<uint32_t>(std::max(minimumPixel.y, 0.0f)) / desc.TileSize, dimensions.y - 1u);
            result.Maximum.x = std::min(static_cast<uint32_t>(std::max(maximumPixel.x, 0.0f)) / desc.TileSize, dimensions.x - 1u);
            result.Maximum.y = std::min(static_cast<uint32_t>(std::max(maximumPixel.y, 0.0f)) / desc.TileSize, dimensions.y - 1u);
            result.Valid = result.Minimum.x <= result.Maximum.x && result.Minimum.y <= result.Maximum.y;
            return result;
        }
    } // namespace

    void ClusteredLightBuilder::Build(const ClusteredLightGridDesc& inputDesc, const glm::mat4& view, const glm::mat4& projection,
                                      const RenderLightData* lights, uint32_t lightCount, ClusteredLightGrid& output)
    {
        ClusteredLightGridDesc desc = inputDesc;
        desc.ViewportWidth = std::max(desc.ViewportWidth, 1u);
        desc.ViewportHeight = std::max(desc.ViewportHeight, 1u);
        desc.TileSize = std::max(desc.TileSize, 1u);
        desc.DepthSlices = std::max(desc.DepthSlices, 1u);
        desc.MaxLightsPerCluster = std::clamp(desc.MaxLightsPerCluster, 1u, 128u);
        desc.MaxDirectionalLights = std::max(desc.MaxDirectionalLights, 1u);
        desc.NearPlane = std::max(desc.NearPlane, 0.0001f);
        desc.FarPlane = std::max(desc.FarPlane, desc.NearPlane + 0.0001f);

        output.Clear();
        output.Dimensions = GetDimensions(desc);
        const uint32_t clusterCount = output.Dimensions.x * output.Dimensions.y * output.Dimensions.z;
        output.Cells.resize(clusterCount);
        Vector<uint32_t> counts(clusterCount, 0u);
        Vector<ClusterBounds> lightBounds(lightCount);

        for (uint32_t lightIndex = 0; lights != nullptr && lightIndex < lightCount; lightIndex++)
        {
            const RenderLightData& light = lights[lightIndex];
            const RenderLightFlags flags = static_cast<RenderLightFlags>(light.Metadata.y);
            if (!HasFlag(flags, RenderLightFlags::Enabled) || !desc.VisibilityMask.Intersects({ light.Metadata.z }))
                continue;

            const LightType type = static_cast<LightType>(light.Metadata.x);
            if (type == LightType::Directional)
            {
                if (output.DirectionalLightIndices.size() < desc.MaxDirectionalLights)
                    output.DirectionalLightIndices.push_back(lightIndex);
                else
                    output.OverflowCount++;
                continue;
            }

            const ClusterBounds bounds =
              ProjectSphere(desc, output.Dimensions, view, projection, glm::vec3(light.PositionRange), light.PositionRange.w);
            lightBounds[lightIndex] = bounds;
            if (!bounds.Valid)
                continue;
            for (uint32_t z = bounds.Minimum.z; z <= bounds.Maximum.z; z++)
                for (uint32_t y = bounds.Minimum.y; y <= bounds.Maximum.y; y++)
                    for (uint32_t x = bounds.Minimum.x; x <= bounds.Maximum.x; x++)
                    {
                        uint32_t& count = counts[Flatten(x, y, z, output.Dimensions)];
                        if (count < desc.MaxLightsPerCluster)
                            count++;
                        else
                            output.OverflowCount++;
                    }
        }

        uint32_t offset = 0;
        for (uint32_t clusterIndex = 0; clusterIndex < clusterCount; clusterIndex++)
        {
            output.Cells[clusterIndex] = { offset, counts[clusterIndex] };
            offset += counts[clusterIndex];
            counts[clusterIndex] = 0;
        }
        output.LightIndices.resize(offset);

        for (uint32_t lightIndex = 0; lightIndex < lightBounds.size(); lightIndex++)
        {
            const ClusterBounds& bounds = lightBounds[lightIndex];
            if (!bounds.Valid)
                continue;
            for (uint32_t z = bounds.Minimum.z; z <= bounds.Maximum.z; z++)
                for (uint32_t y = bounds.Minimum.y; y <= bounds.Maximum.y; y++)
                    for (uint32_t x = bounds.Minimum.x; x <= bounds.Maximum.x; x++)
                    {
                        const uint32_t clusterIndex = Flatten(x, y, z, output.Dimensions);
                        const ClusteredLightCell& cell = output.Cells[clusterIndex];
                        uint32_t& count = counts[clusterIndex];
                        if (count < cell.Count)
                            output.LightIndices[cell.Offset + count++] = lightIndex;
                    }
        }
    }

    ClusteredLightGridDesc ClusteredLightBuilder::ResolveDesc(const RenderPipelineSettings& settings, uint32_t viewportWidth, uint32_t viewportHeight,
                                                              float nearPlane, float farPlane, RenderLayerMask visibilityMask)
    {
        ClusteredLightGridDesc desc;
        desc.ViewportWidth = std::max(viewportWidth, 1u);
        desc.ViewportHeight = std::max(viewportHeight, 1u);
        desc.TileSize = std::max(settings.ClusterTileSize, 1u);
        desc.DepthSlices = std::max(settings.ClusterDepthSlices, 1u);
        // BuildClusteredLights.glsl keeps a fixed-size local list.
        desc.MaxLightsPerCluster = std::clamp(settings.MaxLightsPerCluster, 1u, 128u);
        desc.MaxDirectionalLights = std::max(settings.MaxDirectionalLights, 1u);
        desc.NearPlane = std::max(nearPlane, 0.0001f);
        desc.FarPlane = std::max(farPlane, desc.NearPlane + 0.0001f);
        desc.VisibilityMask = visibilityMask;
        return desc;
    }

    glm::uvec3 ClusteredLightBuilder::GetDimensions(const ClusteredLightGridDesc& inputDesc)
    {
        const uint32_t width = std::max(inputDesc.ViewportWidth, 1u);
        const uint32_t height = std::max(inputDesc.ViewportHeight, 1u);
        const uint32_t tileSize = std::max(inputDesc.TileSize, 1u);
        return { 1u + (width - 1u) / tileSize, 1u + (height - 1u) / tileSize, std::max(inputDesc.DepthSlices, 1u) };
    }

    uint64_t ClusteredLightBuilder::GetClusterCount(const ClusteredLightGridDesc& desc)
    {
        const glm::uvec3 dimensions = GetDimensions(desc);
        return static_cast<uint64_t>(dimensions.x) * dimensions.y * dimensions.z;
    }

    uint32_t ClusteredLightBuilder::DepthToSlice(float depth, float nearPlane, float farPlane, uint32_t depthSlices)
    {
        nearPlane = std::max(nearPlane, 0.0001f);
        farPlane = std::max(farPlane, nearPlane + 0.0001f);
        depthSlices = std::max(depthSlices, 1u);
        const float normalized = std::log(std::clamp(depth, nearPlane, farPlane) / nearPlane) / std::log(farPlane / nearPlane);
        return std::min(static_cast<uint32_t>(normalized * depthSlices), depthSlices - 1u);
    }
} // namespace Crowny
