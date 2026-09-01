#pragma once

#include <cstdint>

namespace Crowny::EnumButtonsModel
{
    bool IsSelected(uint64_t currentValue, uint64_t optionValue, bool flags);
    uint64_t Select(uint64_t currentValue, uint64_t optionValue, bool flags);
} // namespace Crowny::EnumButtonsModel
