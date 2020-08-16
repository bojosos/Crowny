#pragma once

namespace Crowny
{
	class FileSystem
	{
	public:
		static bool FileExists(const std::string& path);
		static int64_t GetFileSize(const std::string& path);

		static byte* ReadFile(const std::string& path);
		static bool ReadFile(const std::string& path, void* buffer, int64_t size = -1);
		static std::string ReadTextFile(const std::string& path);

		static bool WriteFile(const std::string& path, byte* buffer);
		static bool WriteTextFile(const std::string& path, const std::string& text);
	};
}