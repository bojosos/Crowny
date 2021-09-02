
#pragma once

namespace Crowny
{
    class StringUtils
    {
    public:
        static std::vector<std::string> SplitString(const std::string& s, const std::string& separator);

        static int32_t ParseInt(const std::string& value);
        static float ParseFloat(const std::string& value);
        static uint64_t ParseLong(const std::string& value);
        static double ParseDouble(const std::string& value);
        static bool EndWith(const std::string& value, const std::string& end);

        static void ToLower(std::string& string);
        static void ToUpper(std::string& string);

    };
} // namespace Crowny