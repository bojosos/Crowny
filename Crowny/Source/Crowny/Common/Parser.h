#pragma once

namespace Crowny
{
	std::vector<std::string> SplitString(const std::string& s, const std::string& separator);
	const char* FindToken(const char* string, const std::string& token);

	std::vector<std::string> Tokenize(const std::string& string);

	std::string& GetStatement(const char* str, const char** outPosition);
	std::string& GetBlock(const char* str, const char** outPosition);
}