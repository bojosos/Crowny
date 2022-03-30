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

    template <typename T> T UTF8To32(T begin, T end, char32_t& output, char32_t invalid = 0)
    {
        if (begin >= end)
            return begin;
        uint32_t numBytes;
        uint8_t first = (uint8_t)*begin;
        if (first < 192)
            numBytes = 1;
        else if (first < 224)
            numBytes = 2;
        else if (first < 240)
            numBytes = 3;
        else if (first < 248)
            numBytes = 4;
        else if (first < 252)
            numBytes = 5;
        else
            numBytes = 6;
        if (begin + numBytes > end)
        {
            output = invalid;
            return end;
        }

        output = 0;
        switch (numBytes)
        {
        case 6:
            output += (UINT8)(*begin);
            ++begin;
            output <<= 6;
            [[fallthrough]];
        case 5:
            output += (UINT8)(*begin);
            ++begin;
            output <<= 6;
            [[fallthrough]];
        case 4:
            output += (UINT8)(*begin);
            ++begin;
            output <<= 6;
            [[fallthrough]];
        case 3:
            output += (UINT8)(*begin);
            ++begin;
            output <<= 6;
            [[fallthrough]];
        case 2:
            output += (UINT8)(*begin);
            ++begin;
            output <<= 6;
            [[fallthrough]];
        case 1:
            output += (UINT8)(*begin);
            ++begin;
        default:
            break;
        }

        constexpr uint32_t offsets[6] = { 0x00000000, 0x00003080, 0x000E2080, 0x03C82080, 0xFA082080, 0x82082080 };
        output -= offsets[numBytes - 1];

        return begin;
    }

    template <typename T> T UTF16ToUTF32(T begin, T end, char32_t& output, char32_t invalidChar = 0)
    {
        if (begin >= end)
            return begin;

        char16_t firstElem = (char16_t)*begin;
        ++begin;
        if ((firstElem >= 0xD800) && (firstElem <= 0xDBFF))
        {
            if (begin >= end)
            {
                output = invalidChar;
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

    template <typename T> T UTF32ToWide(char32_t input, T output, uint32_t maxElems, wchar_t invalidChar = 0)
    {
        if (sizeof(wchar_t) == 4)
        {
            *output = (wchar_t)input;
            ++output;
            return output;
        }
        else
            return UTF32To16(input, output, maxElems, invalidChar);
    }

    template <typename T> T UTF32To16(char32_t input, T output, uint32_t maxElems, char16_t invalidChar = 0)
    {
        if (maxElems == 0)
            return output;
        if (input > 0x0010FFFF)
        {
            *output = invalidChar;
            ++output;
            return output;
        }

        if (input <= 0xFFFF)
        {
            if ((input >= 0xD800) && (input <= 0xDFFF))
            {
                *output = invalidChar;
                ++output;
                return output;
            }
            *output = (char16_t)input;
            ++output;
        }
        else
        {
            if (maxElems < 2)
            {
                *output = invalidChar;
                ++output;
                return output;
            }
            input -= 0x0010000;

            *output = (char16_t)((input >> 10) + 0xD800);
            ++output;
            *output = (char16_t)((input & 0x3FFUL) + 0xDC00);
            ++output;
        }
        return output;
    }

    template <typename T> T WideToUTF32(T begin, T end, char32_t& output, char32_t invalid = 0)
    {
        if (sizeof(wchar_t) == 4)
        {
            output = (char32_t)*begin;
            ++begin;
            return begin;
        }
        else
            return UTF16ToUTF32(begin, end, output, invalid);
    }

    String UTF8::FromWide(const std::wstring& input)
    {
        String output;
        output.reserve(input.size());
        auto backInserter = std::back_inserter(output);
        auto iter = input.begin();
        while (iter != input.end())
        {
            char32_t u32char;
            iter = WideToUTF32(iter, input.end(), u32char);
            UTF32ToUTF8(u32char, backInserter, 4);
        }
        return output;
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

    std::wstring UTF8::ToWide(const String& string)
    {
        std::wstring output;
        auto back = std::back_inserter(output);
        auto iter = string.begin();
        while (iter != string.end())
        {
            char32_t u32char;
            iter = UTF8To32(iter, string.end(), u32char);
            UTF32ToWide(u32char, back, 2);
        }
        return output;
    }

} // namespace Crowny