#pragma once

namespace Crowny
{

	enum class FileDialogType
	{
		OpenFile = 0,
		OpenFolder = 1,
		SaveFile = 2,
		Multiselect = 3
	};

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

		static bool OpenFileDialog(FileDialogType type, const std::string& initialDir, const std::string& filter, std::vector<std::string>& outpaths);
	};
}