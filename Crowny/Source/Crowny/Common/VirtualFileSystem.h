#pragma once

namespace Crowny
{

    // TODO: make this a module.
    class VirtualFileSystem
    {
    public:
        /**
         * @brief Mounts a directory. Virtual path must start with "/"
         *
         * @param virtualPath Virtual path
         * @param physicalPath Physical path.
         */
        void Mount(const String& virtualPath, const String& physicalPath);
        void Unmount(const String& path);

        bool ResolvePhyiscalPath(const String& path, String& outPath);
        std::tuple<byte*, uint64_t> ReadFile(const String& path);
        String ReadTextFile(const String& path);

        bool WriteFile(const String& path, byte* buff, uint64_t size);
        bool WriteTextFile(const String& path, const String& text);

        static VirtualFileSystem* Get() { return s_Instance; }

    public:
        static void Init();
        static void Shutdown();

    private:
        UnorderedMap<String, Vector<String>> m_MountedDirectories;

    private:
        static VirtualFileSystem* s_Instance;
    };

} // namespace Crowny
