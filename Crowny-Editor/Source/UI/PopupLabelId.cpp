#include "cwepch.h"

#include "UI/PopupLabelId.h"

#include <charconv>
#include <cstring>

namespace Crowny::UI
{
    PopupLabelId PopupLabelId::Create(StringView stem, uint32_t sequence) noexcept
    {
        PopupLabelId id;
        std::memcpy(id.m_Text.data(), stem.data(), stem.size());

        char* cursor = id.m_Text.data() + stem.size();
        *cursor++ = '#';
        *cursor++ = '#';
        const std::to_chars_result result = std::to_chars(cursor, id.m_Text.data() + id.m_Text.size() - 1u, sequence);
        *result.ptr = '\0';
        return id;
    }
} // namespace Crowny::UI
