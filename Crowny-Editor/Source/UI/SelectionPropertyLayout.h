#pragma once

#include <algorithm>
#include <cstddef>

namespace Crowny::UI
{
    struct SelectionVectorLayout
    {
        float LabelWidth = 0.0f;
        float ValueWidth = 0.0f;
        float AxisWidth = 0.0f;
        float InputWidth = 0.0f;
        bool FitsMinimumInputs = false;
    };

    inline SelectionVectorLayout CalculateSelectionVectorLayout(float availableWidth, size_t axisCount, float resetWidth)
    {
        constexpr float preferredLabelWidth = 100.0f;
        constexpr float minimumLabelWidth = 72.0f;
        constexpr float maximumLabelWidth = 120.0f;
        constexpr float minimumInputWidth = 28.0f;
        constexpr float axisSpacing = 3.0f;
        constexpr float resetSpacing = 1.0f;

        SelectionVectorLayout result;
        if (axisCount == 0u)
            return result;

        availableWidth = std::max(availableWidth, 0.0f);
        const float interAxisWidth = static_cast<float>(axisCount - 1u) * axisSpacing;
        const float resetAndInputWidth = resetWidth + (resetWidth > 0.0f ? resetSpacing : 0.0f) + minimumInputWidth;
        const float minimumValueWidth = static_cast<float>(axisCount) * resetAndInputWidth + interAxisWidth;
        const float maximumLabelForInputs = availableWidth - minimumValueWidth;
        result.LabelWidth = std::clamp(std::min(preferredLabelWidth, maximumLabelForInputs), minimumLabelWidth, maximumLabelWidth);
        result.ValueWidth = std::max(availableWidth - result.LabelWidth, 0.0f);
        result.AxisWidth = std::max((result.ValueWidth - interAxisWidth) / static_cast<float>(axisCount), 1.0f);
        result.InputWidth = std::max(result.AxisWidth - resetWidth - (resetWidth > 0.0f ? resetSpacing : 0.0f), 1.0f);
        result.FitsMinimumInputs = result.InputWidth >= minimumInputWidth;
        return result;
    }
} // namespace Crowny::UI
