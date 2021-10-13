#include "cwpch.h"

#include "Crowny/Common/UTF8.h"

namespace Crowny
{

    template <typename T> T UTF32ToUTF8(char32_t input, T output, uint32_t maxElements)
    {
        if ((input > 0x0010FFFF) || ((input >= 0xD800) && input <= 0xDBFF))
        {
            *output = 0;
            ++output;
            return output;
        }

        uint32_t numBytes;
        if (input < 0x80)
            numBytes = 1;
        else if (input < 0x800)
            numBytes = 2;
        else if (input < 0x10000)
            numBytes = 3;
        else
            numBytes = 4;

        if (numBytes > maxElements)
        {
            *output = 0;
            ++output;
            return output;
        }

        constexpr uint8_t headers[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
        char bytes[4];
        switch (numBytes)
        {
        case 4:
            bytes[3] = (char)((input | 0x80) & 0xBF);
            input >>= 6;
        case 3:
            bytes[2] = (char)((input | 0x80) & 0xBF);
            input >>= 6;
        case 2:
            bytes[1] = (char)((input | 0x80) & 0xBF);
            input >>= 6;
        case 1:
            bytes[0] = (char)(input | headers[numBytes]);
        default:
            break;
        }

        output = std::copy(bytes, bytes + numBytes, output);
        return output;
    }

    template <typename T> T UTF16ToUTF32(T begin, T end, char32_t& output)
    {
        if (begin >= end)
            return begin;

        char16_t firstElem = (char16_t)*begin;
        ++begin;
        if ((firstElem >= 0xD800) && (firstElem <= 0xDBFF))
        {
            if (begin >= end)
            {
                output = 0;
                return end;
            }

            char32_t secondElem = (char32_t)*begin;
            ++begin;
            if ((secondElem >= 0xDC00) && (secondElem <= 0xDFFF))
                output = (char32_t)(((firstElem - 0xD800) << 10) + (secondElem - 0xDC00) + 0x0010000);
            else
                output = 0;
        }
        else
        {
            output = (char32_t)firstElem;
            return begin;
        }

        return begin;
    }

    String UTF8::FromUTF16(const U16String& string)
    {
        String output;
        output.reserve(string.size());
        auto back = std::back_inserter(output);

        auto iter = string.begin();
        while (iter != string.end())
        {
            char32_t u32Char = 0;
            iter = UTF16ToUTF32(iter, string.end(), u32Char);
            UTF32ToUTF8(u32Char, back, 4);
        }
        return output;
    }

    String UTF8::FromUTF32(const U32String& string)
    {
        String output;
        output.reserve(string.size());

        auto back = std::back_inserter(output);

        auto iter = string.begin();
        while (iter != string.end())
        {
            UTF32ToUTF8(*iter, back, 4);
            ++iter;
        }

        return output;
    }

} // namespace Crowny