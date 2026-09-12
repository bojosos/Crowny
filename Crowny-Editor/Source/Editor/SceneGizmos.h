#pragma once

#include "Crowny/Ecs/Entity.h"

#include <imgui.h>

namespace Crowny
{
    struct SceneGizmoSettings
    {
        bool Enabled = true;
        bool Lights = true;
        bool Cameras = true;
        bool Audio = true;
        bool SelectionGuides = true;
        float IconSize = 24.0f;
    };

    namespace SceneGizmos
    {
        void DrawIcons(Scene& scene, const glm::mat4& viewProjection, const glm::vec4& bounds, const SceneGizmoSettings& settings,
                       const Vector<Entity>& selection);
        Entity PickIcon(Scene& scene, const glm::mat4& viewProjection, const glm::vec4& bounds, const SceneGizmoSettings& settings,
                        const glm::vec2& cursor);
        // Called inside the editor's depth-tested Renderer2D batch.
        void DrawSelectionGuides(const Vector<Entity>& selection, const SceneGizmoSettings& settings);
    } // namespace SceneGizmos
} // namespace Crowny
