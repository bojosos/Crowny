#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"

#include <algorithm>

namespace Crowny
{

    std::vector<std::string> StringUtils::SplitString(const std::string& s, const std::string& separator)
    {
        std::vector<std::string> output;

        std::string::size_type start = 0, end = s.find_first_of(separator);

        while ((end <= std::string::npos))
        {
            std::string tok = s.substr(start, end - start);
            if (!tok.empty())
                output.push_back(tok);

            if (end == std::string::npos)
                break;

            start = end + 1;
            end = s.find_first_of(separator, start);
        }

        return output;
    }

    int32_t StringUtils::ParseInt(const std::string& value) { return std::stoi(value); }

    float StringUtils::ParseFloat(const std::string& value) { return std::stof(value); }

    uint64_t StringUtils::ParseLong(const std::string& value) { return std::stoll(value); }

    double StringUtils::ParseDouble(const std::string& value) { return std::stod(value); }

    bool StringUtils::EndWith(const std::string& value, const std::string& end)
    {
        if (end.size() > value.size())
            return false;
        return std::equal(end.rbegin(), end.rend(), value.rbegin());
    }
    
    void StringUtils::ToLower(std::string& string)
    {
        std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::tolower(c); });
    }

    void StringUtils::ToUpper(std::string& string)
    {
        std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::toupper(c); });
    }

} // namespace Crowny
