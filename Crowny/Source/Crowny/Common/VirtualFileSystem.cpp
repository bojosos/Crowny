#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

    static String FixPath(const String& path)
    {
        String res = path;
        std::replace(res.begin(), res.end(), '\\', '/');
        
        // Normalize: resolve /./ and /../
        Vector<String> parts = StringUtils::SplitString(res, "/");
        Vector<String> resultParts;
        
        for (const auto& part : parts)
        {
            if (part == "." || part == "") continue;
            if (part == "..")
            {
                if (!resultParts.empty()) resultParts.pop_back();
            }
            else
            {
                resultParts.push_back(part);
            }
        }

        String normalized = "/";
        for (size_t i = 0; i < resultParts.size(); ++i)
        {
            normalized += resultParts[i];
            if (i < resultParts.size() - 1) normalized += "/";
        }
        
        return normalized;
    }

    void VirtualFileSystem::Mount(const String& virtualPath, const String& physicalPath)
    {
        const String vPath = FixPath(virtualPath);
        String pPath = physicalPath;
        std::replace(pPath.begin(), pPath.end(), '\\', '/');
        if (!pPath.empty() && pPath.back() != '/') pPath += "/";

        m_MountedDirectories[vPath].push_back(pPath);
    }

    void VirtualFileSystem::Unmount(const String& path)
    {
        m_MountedDirectories.erase(FixPath(path));
    }

    bool VirtualFileSystem::Exists(const String& path)
    {
        String physPath;
        return ResolvePhyiscalPath(path, physPath);
    }

    uint64_t VirtualFileSystem::GetSize(const String& path)
    {
        String physPath;
        if (ResolvePhyiscalPath(path, physPath))
            return FileSystem::GetFileSize(physPath);
        return 0;
    }

    bool VirtualFileSystem::ResolvePhyiscalPath(const String& inPath, String& outPath, bool forWrite)
    {
        const String virtualPath = FixPath(inPath);
        
        // Find matching mount point
        // Basic implementation: find longest matching prefix
        String bestMatch;
        for (auto const& [vPath, pPaths] : m_MountedDirectories)
        {
            if (virtualPath.find(vPath) == 0)
            {
                if (vPath.size() > bestMatch.size())
                    bestMatch = vPath;
            }
        }

        if (bestMatch.empty())
        {
            // If not found in VFS, check if it's a direct physical path (absolute)
            // This is kept for compatibility with existing code
            if (FileSystem::FileExists(inPath))
            {
                outPath = inPath;
                return true;
            }
            return false;
        }

        String remaining = virtualPath.substr(bestMatch.size());
        if (!remaining.empty() && remaining[0] == '/')
            remaining = remaining.substr(1);

        const auto& physicalPaths = m_MountedDirectories[bestMatch];
        
        if (forWrite)
        {
            // For writing, we use the first (highest priority) mount point
            outPath = physicalPaths[0] + remaining;
            return true;
        }

        // For reading, search through all overlays
        for (const auto& physRoot : physicalPaths)
        {
            const String candidate = physRoot + remaining;
            if (FileSystem::FileExists(candidate))
            {
                outPath = candidate;
                return true;
            }
        }

        return false;
    }

    std::tuple<byte*, uint64_t> VirtualFileSystem::ReadFile(const String& path)
    {
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadFile(phPath) : std::make_tuple(nullptr, (uint64_t)-1);
    }

    String VirtualFileSystem::ReadTextFile(const String& path)
    {
        String phPath;
        return ResolvePhyiscalPath(path, phPath) ? FileSystem::ReadTextFile(phPath) : String();
    }

    bool VirtualFileSystem::WriteFile(const String& path, byte* buff, uint64_t size)
    {
        String phPath;
        return ResolvePhyiscalPath(path, phPath, true) ? FileSystem::WriteFile(phPath, buff, size) : false;
    }

    bool VirtualFileSystem::WriteTextFile(const String& path, const String& text)
    {
        String phPath;
        return ResolvePhyiscalPath(path, phPath, true) ? FileSystem::WriteTextFile(phPath, text) : false;
    }

} // namespace Crowny
