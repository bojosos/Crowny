#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
    struct ConsoleSeverityToggleVisual
    {
        glm::vec4 Text;
        glm::vec4 Fill;
        glm::vec4 HoveredFill;
        glm::vec4 ActiveFill;
        glm::vec4 Border;
    };

    inline ConsoleSeverityToggleVisual BuildConsoleSeverityToggleVisual(const glm::vec4& severity, bool enabled)
    {
        const auto withOpacity = [&severity](float opacity) { return glm::vec4{ severity.r, severity.g, severity.b, severity.a * opacity }; };

        return {
            withOpacity(enabled ? 1.00f : 0.46f), withOpacity(enabled ? 0.16f : 0.00f), withOpacity(enabled ? 0.24f : 0.10f),
            withOpacity(enabled ? 0.32f : 0.18f), withOpacity(enabled ? 0.72f : 0.00f),
        };
    }
} // namespace Crowny
