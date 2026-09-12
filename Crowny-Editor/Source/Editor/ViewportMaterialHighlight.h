#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Renderer/Mesh.h"

#include <cmath>
#include <optional>

namespace Crowny
{
    inline AssetHandle<Mesh> GetMaterialDropMesh(Entity entity)
    {
        if (!entity)
            return {};
        if (entity.HasComponent<MeshRendererComponent>())
        {
            if (entity.HasComponent<AnimationComponent>() && entity.GetComponent<AnimationComponent>().RuntimeMeshHandle)
                return entity.GetComponent<AnimationComponent>().RuntimeMeshHandle;
            return entity.GetComponent<MeshRendererComponent>().MeshHandle;
        }
        if (entity.HasComponent<ProceduralMeshComponent>())
            return entity.GetComponent<ProceduralMeshComponent>().RuntimeMeshHandle;
        return {};
    }

    // Clip each bounds edge before projection, including edges that cross the near plane.
    inline std::optional<Pair<glm::vec2, glm::vec2>> ProjectMaterialHighlightEdge(glm::vec4 first, glm::vec4 second, const glm::vec4& viewport)
    {
        if (viewport.z <= viewport.x || viewport.w <= viewport.y)
            return std::nullopt;
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            for (const float sign : { -1.0f, 1.0f })
            {
                const float start = first.w + sign * first[axis];
                const float end = second.w + sign * second[axis];
                if (!std::isfinite(start) || !std::isfinite(end) || (start < 0.0f && end < 0.0f))
                    return std::nullopt;
                if ((start < 0.0f) != (end < 0.0f))
                {
                    const glm::vec4 intersection = glm::mix(first, second, start / (start - end));
                    if (start < 0.0f)
                        first = intersection;
                    else
                        second = intersection;
                }
            }
        }
        if (first.w <= 0.00001f || second.w <= 0.00001f)
            return std::nullopt;
        const auto project = [&](const glm::vec4& point) {
            const glm::vec2 ndc = glm::vec2(point) / point.w;
            return glm::vec2(viewport.x + (ndc.x + 1.0f) * 0.5f * (viewport.z - viewport.x),
                             viewport.y + (1.0f - ndc.y) * 0.5f * (viewport.w - viewport.y));
        };
        return Pair{ project(first), project(second) };
    }
} // namespace Crowny
