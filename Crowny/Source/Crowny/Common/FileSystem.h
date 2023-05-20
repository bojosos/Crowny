#pragma once

#include "Crowny/Common/DataStream.h"

namespace Crowny
{

    enum class FileDialogType
    {
        OpenFile = 0,
        OpenFolder = 1,
        SaveFile = 2,
        Multiselect = 3
    };

    struct DialogFilter
    {
        String Name;
        String FilterSpec;
    };

    class FileSystem
    {
    public:
        static bool FileExists(const Path& path);
        static uintmax_t GetFileSize(const Path& path);

        static std::tuple<byte*, uint64_t> ReadFile(const Path& path);
        static bool ReadFile(const Path& path, void* buffer, int64_t size = -1);
        static String ReadTextFile(const Path& path);

        static bool WriteFile(const Path& path, byte* buffer, uint64_t size);

        static bool WriteTextFile(const Path& path, const String& text);
        static bool OpenFileDialog(FileDialogType type, Vector<Path>& outpaths, const String& title = {},
                                   const Path& initialDir = {}, const Vector<DialogFilter>& filters = {},
                                   const String& filename = {});

        static Ref<DataStream> OpenFile(const Path& filepath, bool readOnly = true);
        static Ref<DataStream> CreateAndOpenFile(const Path& filepath);
    };
} // namespace Crowny
