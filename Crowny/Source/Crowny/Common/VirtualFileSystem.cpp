#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

    VirtualFileSystem* VirtualFileSystem::s_Instance = nullptr;

    static String FixPath(const String& badPath)
    {
        // TODO: Replace all?
        String res = badPath;
        size_t startPos = 0;
        while ((startPos = res.find("\\", startPos)) != String::npos)
        {
            res.replace(startPos, 1, "/");
            startPos++;
        }
        return res;
    }

    void VirtualFileSystem::Init() { s_Instance = new VirtualFileSystem(); }

    void VirtualFileSystem::Shutdown() { delete s_Instance; }

    void VirtualFileSystem::Mount(const String& virtualPath, const String& physicalPath)
    {
        CW_ENGINE_ASSERT(s_Instance);
        m_MountedDirectories[virtualPath].push_back(physicalPath);
    }

    void VirtualFileSystem::Unmount(const String& path)
    {
        CW_ENGINE_ASSERT(s_Instance);
        m_MountedDirectories[path].clear();
    }

    bool VirtualFileSystem::ResolvePhyiscalPath(const String& inPath, String& outPath)
    {
        String virtualPath = FixPath(inPath);
        if (virtualPath[0] != '/')
        {
            outPath = virtualPath;
            return true;
        }

        Vector<String> dirs = StringUtils::SplitString(virtualPath, "/");
        const String& virtualDir = dirs.front();

        if (m_MountedDirectories.find(virtualDir) == m_MountedDirectories.end() ||
            m_MountedDirectories[virtualDir].empty())
        // return false;
        {
#ifdef CW_PLATFORM_LINUX // on linux full paths start with /
            outPath = virtualPath;
            return true; // ?
#endif
            CW_ENGINE_WARN("File {0} does not exist", virtualPath);
        }

        String remaining = virtualPath.substr(virtualDir.size() + 1, virtualPath.size() - virtualDir.size());
        for (const String& phPath : m_MountedDirectories[virtualDir])
        {
            String path = phPath + remaining;
            outPath = path;
            return true;
        }

        CW_ENGINE_WARN("File {0} does not exist", virtualPath);
        return false;
    }

    std::tuple<byte*, uint64_t> VirtualFileSystem::ReadFile(const String& path)
    {
        CW_ENGINE_ASSERT(s_Instance);
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadFile(phPath) : std::make_tuple(nullptr, -1);
    }

    String VirtualFileSystem::ReadTextFile(const String& path)
    {
        CW_ENGINE_ASSERT(s_Instance);
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadTextFile(phPath) : String();
    }

    bool VirtualFileSystem::WriteFile(const String& path, byte* buff, uint64_t size)
    {
        CW_ENGINE_ASSERT(s_Instance);
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteFile(phPath, buff, size) : false;
    }

    bool VirtualFileSystem::WriteTextFile(const String& path, const String& text)
    {
        CW_ENGINE_ASSERT(s_Instance);
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::WriteTextFile(phPath, text) : false;
    }

} // namespace Crowny
