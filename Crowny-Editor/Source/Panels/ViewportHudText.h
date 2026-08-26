#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    struct ViewportHudStatus
    {
        static constexpr size_t Capacity = 128u;

        Array<char, Capacity> Text{};
    };

    ViewportHudStatus FormatViewportHudStatus(StringView primaryName, bool hasPrimary, size_t selectionCount, int32_t viewportWidth,
                                              int32_t viewportHeight, float cameraDistance) noexcept;
} // namespace Crowny
