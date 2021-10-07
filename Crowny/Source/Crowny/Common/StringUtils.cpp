#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"

#include <algorithm>

namespace Crowny
{

    Vector<String> StringUtils::SplitString(const String& s, const String& separator)
    {
        Vector<String> output;

        String::size_type start = 0, end = s.find_first_of(separator);

        while ((end <= String::npos))
        {
            String tok = s.substr(start, end - start);
            if (!tok.empty())
                output.push_back(tok);

            if (end == String::npos)
                break;

            start = end + 1;
            end = s.find_first_of(separator, start);
        }

        return output;
    }

    int32_t StringUtils::ParseInt(const String& value) { return std::strtol(value.c_str(), nullptr, 10); }

    float StringUtils::ParseFloat(const String& value) { return std::strtof(value.c_str(), nullptr); }

    uint64_t StringUtils::ParseLong(const String& value) { return std::strtoull(value.c_str(), nullptr, 10); }

    double StringUtils::ParseDouble(const String& value) { return std::strtold(value.c_str(), nullptr); }

    bool StringUtils::EndsWith(const String& value, const String& end)
    {
        if (end.size() > value.size())
            return false;
        return std::equal(end.rbegin(), end.rend(), value.rbegin());
    }

    void StringUtils::ToLower(String& string)
    {
        std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::tolower(c); });
    }

    void StringUtils::ToUpper(String& string)
    {
        std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::toupper(c); });
    }

} // namespace Crowny
