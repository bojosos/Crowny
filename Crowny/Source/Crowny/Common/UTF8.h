#pragma once

namespace Crowny
{
    class UTF8
    {
    public:
        static String FromUTF16(const U16String& string);
        static String FromUTF32(const U32String& string);
        static std::wstring ToWide(const String& string);
        static String FromWide(const std::wstring& string);
    };
} // namespace Crowny