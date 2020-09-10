#pragma once

namespace Crowny
{
	class FileSystem
	{
	public:
		static bool FileExists(const std::string& path);
		static int64_t GetFileSize(const std::string& path);

		static std::tuple<byte*, uint64_t> ReadFile(const std::string& path);
		static bool ReadFile(const std::string& path, void* buffer, int64_t size = -1);
		static std::string ReadTextFile(const std::string& path);

		static bool WriteFile(const std::string& path, byte* buffer);
		static bool WriteTextFile(const std::string& path, const std::string& text);

		// Returns a tuple<success, path of the selected file>
		static std::tuple<bool, std::string> OpenFileDialog(const char* = "All\0*.*\0\0", const std::string& initialDir = "", const std::string& title = "Open File");

		// Returns a tuple<success, path to be saved>
		static std::tuple<bool, std::string> SaveFileDialog(const char* = "All\0*.*\0\0", const std::string& initialDir = "", const std::string& title = "Save File");
	};
}