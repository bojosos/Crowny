#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{

    class VirtualFileSystem : public Module<VirtualFileSystem>
    {
    public:
        void Mount(const String& virtualPath, const String& physicalPath);
        void Unmount(const String& path);

        bool Exists(const String& path);
        uint64_t GetSize(const String& path);

        bool ResolvePhyiscalPath(const String& path, String& outPath, bool forWrite = false);
        std::tuple<byte*, uint64_t> ReadFile(const String& path);
        String ReadTextFile(const String& path);

        bool WriteFile(const String& path, byte* buff, uint64_t size);
        bool WriteTextFile(const String& path, const String& text);

        static VirtualFileSystem* Get() { return GetPtr(); }

    private:
        UnorderedMap<String, Vector<String>> m_MountedDirectories;
    };

} // namespace Crowny
