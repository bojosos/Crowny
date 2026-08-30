#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"

#include <spdlog/fmt/fmt.h>

namespace Crowny
{
    namespace
    {
        bool NextSearchCharacter(StringView text, size_t& offset, bool caseSensitive, bool stripWhiteSpaces, bool stripUnderscores,
                                 unsigned char& output)
        {
            while (offset < text.size())
            {
                unsigned char value = static_cast<unsigned char>(text[offset++]);
                if (stripUnderscores && value == '_')
                    value = ' ';
                if (stripWhiteSpaces && value == ' ')
                    continue;

                output = caseSensitive ? value : static_cast<unsigned char>(std::tolower(value));
                return true;
            }
            return false;
        }

        bool ContainsSearchTerm(StringView item, StringView searchTerm, bool caseSensitive, bool stripWhiteSpaces, bool stripUnderscores)
        {
            size_t termOffset = 0;
            unsigned char termCharacter = 0;
            if (!NextSearchCharacter(searchTerm, termOffset, caseSensitive, stripWhiteSpaces, false, termCharacter))
                return true;

            for (size_t start = 0; start < item.size(); ++start)
            {
                size_t itemOffset = start;
                termOffset = 0;
                bool matches = true;
                while (NextSearchCharacter(searchTerm, termOffset, caseSensitive, stripWhiteSpaces, false, termCharacter))
                {
                    unsigned char itemCharacter = 0;
                    if (!NextSearchCharacter(item, itemOffset, caseSensitive, stripWhiteSpaces, stripUnderscores, itemCharacter) ||
                        itemCharacter != termCharacter)
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                    return true;
            }
            return false;
        }
    } // namespace

    bool StringUtils::IsSearchMathing(StringView item, StringView searchQuery, bool caseSensitive, bool stripWhiteSpaces, bool stripUnderscores)
    {
        if (searchQuery.empty())
            return true;

        if (item.empty())
            return false;

        if (!stripWhiteSpaces && searchQuery.find(' ') != StringView::npos)
        {
            bool foundTerm = false;
            size_t termBegin = 0;
            while (termBegin < searchQuery.size())
            {
                while (termBegin < searchQuery.size() && searchQuery[termBegin] == ' ')
                    ++termBegin;
                if (termBegin == searchQuery.size())
                    break;

                const size_t termEnd = searchQuery.find(' ', termBegin);
                const StringView term = searchQuery.substr(termBegin, termEnd - termBegin);
                foundTerm = true;
                if (!ContainsSearchTerm(item, term, caseSensitive, false, stripUnderscores))
                    return false;
                if (termEnd == StringView::npos)
                    break;
                termBegin = termEnd + 1u;
            }
            return foundTerm;
        }

        return ContainsSearchTerm(item, searchQuery, caseSensitive, stripWhiteSpaces, stripUnderscores);
    }

    Vector<String> StringUtils::SplitString(const String& s, const String& separator)
    {
        Vector<String> output;

        String::size_type start = 0, end = s.find_first_of(separator);

        while ((end <= String::npos))
        {
            const String tok = s.substr(start, end - start);
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
            const std::wstring tok = s.substr(start, end - start);
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
        return lhs.size() == rhs.size() &&
               std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](unsigned char l, unsigned char r) { return std::tolower(l) == std::tolower(r); });
    }

} // namespace Crowny
