#include "cwepch.h"

#include "Panels/ViewportHudText.h"

#include <cstdio>

namespace Crowny
{
    ViewportHudStatus FormatViewportHudStatus(StringView primaryName, bool hasPrimary, size_t selectionCount, int32_t viewportWidth,
                                              int32_t viewportHeight, float cameraDistance) noexcept
    {
        ViewportHudStatus status;

        if (selectionCount > 1u)
        {
            std::snprintf(status.Text.data(), status.Text.size(), "%llu entities  |  %d x %d  |  View %.1f m",
                          static_cast<unsigned long long>(selectionCount), viewportWidth, viewportHeight, static_cast<double>(cameraDistance));
        }
        else if (hasPrimary)
        {
            const bool truncateName = primaryName.size() > 28u;
            const size_t nameLength = truncateName ? 25u : primaryName.size();
            const char* name = primaryName.empty() ? "" : primaryName.data();
            std::snprintf(status.Text.data(), status.Text.size(), "%.*s%s  |  %d x %d  |  View %.1f m", static_cast<int>(nameLength), name,
                          truncateName ? "..." : "", viewportWidth, viewportHeight, static_cast<double>(cameraDistance));
        }
        else
        {
            std::snprintf(status.Text.data(), status.Text.size(), "No selection  |  %d x %d  |  View %.1f m", viewportWidth, viewportHeight,
                          static_cast<double>(cameraDistance));
        }

        status.Text.back() = '\0';
        return status;
    }
} // namespace Crowny
