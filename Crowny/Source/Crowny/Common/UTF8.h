#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{
    class UTF8
    {
    public:
        static String FromUTF16(const U16String& string);
        static String FromUTF32(const U32String& string);
        static U32String ToUTF32(StringView string);
        static std::wstring ToWide(const String& string);
        static String FromWide(const std::wstring& string);

        static bool NextCodePoint(StringView string, size_t& offset, char32_t& codePoint, char32_t replacement = 0xFFFD);
    };
} // namespace Crowny
