#pragma once

namespace Crowny
{
    class StringUtils
    {
    public:
        static Vector<String> SplitString(const String& s, const String& separator);
        static Vector<std::wstring> SplitString(const std::wstring& s, const std::wstring& separator);

        static String Replace(const String& s, const String& from, const String& to);

        static int32_t ParseInt(const String& value);
        static float ParseFloat(const String& value);
        static uint64_t ParseLong(const String& value);
        static double ParseDouble(const String& value);
        static bool EndsWith(const String& value, const String& end);

        static void ToLower(String& string);
        static void ToUpper(String& string);

        static bool IsSearchMathing(const String& item, const String& searchQuery, bool caseSensitive = false,
                                    bool stripWhiteSpaces = false, bool stripUnderscores = false);
    };
} // namespace Crowny