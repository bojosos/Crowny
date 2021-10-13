#pragma once

namespace Crowny
{
    class UTF8
    {
    public:
        static String FromUTF16(const U16String& string);
        static String FromUTF32(const U32String& string);
    };
} // namespace Crowny