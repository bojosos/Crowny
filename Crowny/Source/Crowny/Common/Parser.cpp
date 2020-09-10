#include "cwpch.h"

#include "Crowny/Common/Parser.h"

namespace Crowny
{

	std::vector<std::string> SplitString(const std::string& s, const std::string& separator)
	{
		std::vector<std::string> output;

		std::string::size_type start = 0, end = s.find_first_of(separator);

		while (end <= std::string::npos)
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

	const char* FindToken(const char* str, const std::string& token)
	{
		const char* t = str;
		while (t = strstr(t, token.c_str()))
		{
			bool left = str == t || isspace(t[-1]);
			bool right = !t[token.size()] || isspace(t[token.size()]);
			if (left && right)
				return t;

			t += token.size();
		}
		return nullptr;
	}

	std::string GetBlock(const char* str, const char** outPosition)
	{
		const char* end = strstr(str, "}");
		if (!end)
			return std::string(str);

		if (outPosition)
			*outPosition = end;
		uint32_t length = end - str + 1;
		return std::string(str, length);
	}

	std::string GetStatement(const char* str, const char** outPosition)
	{
		const char* end = strstr(str, ";");
		if (!end)
			return std::string(str);

		if (outPosition)
			*outPosition = end;
		uint32_t length = end - str + 1;
		std::string res = std::string(str, length);
		return res;
	}

	std::vector<std::string> Tokenize(const std::string& string)
	{
		return SplitString(string, " \t\n");
	}

}
