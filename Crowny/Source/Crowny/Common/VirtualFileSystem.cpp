#include "cwpch.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

	VirtualFileSystem* VirtualFileSystem::s_Instance = nullptr;

	static std::string FixPath(const std::string& badPath)
	{
		// TODO: Replace all?
		std::string res = badPath;
		size_t startPos = 0;
		while((startPos = res.find("\\", startPos)) != std::string::npos) {
			res.replace(startPos, 1, "/");
			startPos++;
		}
		return res;
	}

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
		CW_ENGINE_ASSERT(s_Instance);
		m_MountedDirectories[virtualPath].push_back(physicalPath);
	}

	void VirtualFileSystem::Unmount(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance);
		m_MountedDirectories[path].clear();
	}

	bool VirtualFileSystem::ResolvePhyiscalPath(const std::string& inPath, std::string& outPath)
	{
		std::string virtualPath = FixPath(inPath);
		if (virtualPath[0] != '/')
		{
			outPath = virtualPath;
			return true;
		}

		std::vector<std::string> dirs = StringUtils::SplitString(virtualPath, "/");
		const std::string& virtualDir = dirs.front();

		if (m_MountedDirectories.find(virtualDir) == m_MountedDirectories.end() || m_MountedDirectories[virtualDir].empty())
		//return false;
		{
#ifdef CW_PLATFORM_LINUX // on linux full paths start with /
			outPath = virtualPath;
#endif
			CW_ENGINE_WARN("File {0} does not exist", virtualPath);
		}

		std::string remaining = virtualPath.substr(virtualDir.size() + 1, virtualPath.size() - virtualDir.size());
		for (const std::string& phPath : m_MountedDirectories[virtualDir])
		{
			std::string path = phPath + remaining;
			outPath = path;
			return true;
		}

		CW_ENGINE_WARN("File {0} does not exist", virtualPath);
		return false;
	}

	std::tuple<byte*, uint64_t> VirtualFileSystem::ReadFile(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance);
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadFile(phPath) : std::make_tuple(nullptr, -1);
	}

	std::string VirtualFileSystem::ReadTextFile(const std::string& path)
	{
		CW_ENGINE_ASSERT(s_Instance);
		std::string phPath;		
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadTextFile(phPath) : std::string();
	}

	bool VirtualFileSystem::WriteFile(const std::string& path, byte* buff, uint64_t size)
	{
		CW_ENGINE_ASSERT(s_Instance);
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteFile(phPath, buff, size) : false;
	}

	bool VirtualFileSystem::WriteTextFile(const std::string& path, const std::string& text)
	{
		CW_ENGINE_ASSERT(s_Instance);
		std::string phPath;
		return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteTextFile(phPath, text) : false;
	}

}