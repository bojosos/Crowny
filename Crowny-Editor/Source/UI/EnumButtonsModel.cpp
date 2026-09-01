#include "cwepch.h"

#include "UI/EnumButtonsModel.h"

namespace Crowny::EnumButtonsModel
{
    bool IsSelected(uint64_t currentValue, uint64_t optionValue, bool flags)
    {
        if (!flags)
            return currentValue == optionValue;
        if (optionValue == 0)
            return currentValue == 0;
        return (currentValue & optionValue) == optionValue;
    }

    uint64_t Select(uint64_t currentValue, uint64_t optionValue, bool flags)
    {
        if (!flags)
            return optionValue;
        if (optionValue == 0)
            return 0;
        return IsSelected(currentValue, optionValue, true) ? currentValue & ~optionValue : currentValue | optionValue;
    }
} // namespace Crowny::EnumButtonsModel
