#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny::UI
{
    class PopupLabelId
    {
    public:
        static constexpr size_t Capacity = 32u;
        static constexpr size_t SeparatorLength = 2u;
        static constexpr size_t MaximumSequenceLength = 10u;
        static constexpr size_t MaximumStemLength = Capacity - SeparatorLength - MaximumSequenceLength - 1u;

        template <size_t N> static PopupLabelId Create(const char (&stem)[N], uint32_t sequence) noexcept
        {
            static_assert(N > 0u);
            static_assert(N - 1u <= MaximumStemLength, "Popup ID stem exceeds fixed storage");
            return Create(StringView(stem, N - 1u), sequence);
        }

        const char* CStr() const noexcept { return m_Text.data(); }
        StringView View() const noexcept { return m_Text.data(); }

    private:
        static PopupLabelId Create(StringView stem, uint32_t sequence) noexcept;

        Array<char, Capacity> m_Text{};
    };
} // namespace Crowny::UI
