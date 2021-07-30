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
		
		static const char* FindToken(const char* string, const std::string& token);

		static std::vector<std::string> Tokenize(const std::string& string);

		static std::string GetStatement(const char* str, const char** outPosition);
		static std::string GetBlock(const char* str, const char** outPosition);
	};
}