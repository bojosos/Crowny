#pragma once

namespace Crowny
{
	class VirtualFileSystem
	{
	public:
		void Mount(const std::string& virtualPath, const std::string& physicalPath);
		void Unmount(const std::string& path);

		bool ResolvePhyiscalPath(const std::string& path, std::string& outPath);
		std::tuple<byte*, uint64_t> ReadFile(const std::string& path);
		std::string ReadTextFile(const std::string& path);

		bool WriteFile(const std::string& path, byte* buff);
		bool WriteTextFile(const std::string& path, const std::string& text);

		static VirtualFileSystem* Get()
		{
			return s_Instance;
		}

	public:
		static void Init();
		static void Shutdown();
	private:
		std::unordered_map<std::string, std::vector<std::string>> m_MountedDirectories;
	private:
		static VirtualFileSystem* s_Instance;
	};
}