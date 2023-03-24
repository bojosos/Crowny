#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"

#include <spdlog/fmt/fmt.h>

namespace Crowny
{

    bool StringUtils::IsSearchMathing(const String& item, const String& searchQuery, bool caseSensitive,
                                      bool stripWhiteSpaces, bool stripUnderscores)
    {
        if (searchQuery.empty())
            return true;

        if (item.empty())
            return false;

        String itemSanitized = stripUnderscores ? StringUtils::Replace(item, "_", " ") : item;

        if (stripWhiteSpaces)
            itemSanitized = StringUtils::Replace(itemSanitized, " ", "");

        String searchString = stripWhiteSpaces ? StringUtils::Replace(searchQuery, " ", "") : String(searchQuery);

        if (!caseSensitive)
        {
            StringUtils::ToLower(itemSanitized);
            StringUtils::ToLower(searchString);
        }

        bool result = false;
        if (searchString.find(" ") != String::npos)
        {
            Vector<String> searchTerms = SplitString(searchString, " ");
            for (const auto& searchTerm : searchTerms)
            {
                if (!searchTerm.empty() && itemSanitized.find(searchTerm) != String::npos)
                    result = true;
                else
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            result = itemSanitized.find(searchString) != String::npos;
        }

        return result;
    }

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

    Vector<std::wstring> StringUtils::SplitString(const std::wstring& s, const std::wstring& separator)
    {
        Vector<std::wstring> output;

        std::wstring::size_type start = 0, end = s.find_first_of(separator);

        while (end <= String::npos)
        {
            std::wstring tok = s.substr(start, end - start);
            if (!tok.empty())
                output.push_back(tok);

            if (end == std::wstring::npos)
                break;

            start = end + 1;
            end = s.find_first_of(separator, start);
        }

        return output;
    }

    String StringUtils::Replace(const String& str, const String& from, const String& to)
    {
        String result = str;
        if (from.empty())
            return String();
        size_t startPos = 0;
        while ((startPos = result.find(from, startPos)) != String::npos)
        {
            result.replace(startPos, from.length(), to);
            startPos += to.length();
        }
        return result;
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

    bool StringUtils::CaseInsensitiveCompare(const String& lhs, const String& rhs)
    {
        const auto result =
          std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(),
                        [](const unsigned char l, const unsigned char r) { return std::tolower(l) == tolower(r); });
        return result.second != rhs.cend() &&
               (result.first == lhs.cend() || tolower(*result.first) < tolower(*result.second));
    }

} // namespace Crowny
