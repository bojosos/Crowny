#include "cwpch.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Parser.h"

namespace Crowny
{

	VirtualFileSystem* VirtualFileSystem::s_Instance = nullptr;

	void VirtualFileSystem::Init()
	{
		s_Instance = new VirtualFileSystem();
	}

	void VirtualFileSystem::Shutdown()
	{
		delete s_Instance;
	}

	void VirtualFileSystem::Mount(const std::string& virtualPath, const std::string& physicalPath)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		m_MountedDirectories[virtualPath].push_back(physicalPath);
	}

	void VirtualFileSystem::Unmount(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		m_MountedDirectories[path].clear();
	}

	bool VirtualFileSystem::ResolvePhyiscalPath(const std::string& virtualPath, std::string& outPath)
	{
		if (virtualPath[0] != '/')
		{
			outPath = virtualPath;
			return FileSystem::FileExists(virtualPath);
		}

		std::vector<std::string> dirs = SplitString(virtualPath, "/");
		const std::string& virtualDir = dirs.front();

		if (m_MountedDirectories.find(virtualDir) == m_MountedDirectories.end() || m_MountedDirectories[virtualDir].empty())
			return false;

		std::string remaining = virtualPath.substr(virtualDir.size() + 1, virtualPath.size() - virtualDir.size());
		for (const std::string& phPath : m_MountedDirectories[virtualDir])
		{
			std::string& path = phPath + remaining;
			if (FileSystem::FileExists(path))
			{
				outPath = path;
				return true;
			}
		}

		return false;
	}

	std::tuple<byte*, uint64_t> VirtualFileSystem::ReadFile(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadFile(phPath) : std::make_tuple(nullptr, -1);
	}

	std::string VirtualFileSystem::ReadTextFile(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadTextFile(phPath) : nullptr;
	}

	bool VirtualFileSystem::WriteFile(const std::string& path, byte* buff)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteFile(phPath, buff) : nullptr;
	}

	bool VirtualFileSystem::WriteTextFile(const std::string& path, const std::string& text)
	{
		CW_ENGINE_ASSERT(s_Instance, "");
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteTextFile(phPath, text) : nullptr;
	}

}