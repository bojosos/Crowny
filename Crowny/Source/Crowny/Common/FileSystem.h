#pragma once

#include "Crowny/Common/DataStream.h"

namespace Crowny
{

    enum class FileDialogType // TODO: Make this enum better
    {
        OpenFile,
        OpenFolder,
        SaveFile,
        Multiselect
    };

    class FileSystem
    {
    public:
        static bool FileExists(const Path& path);
        static int64_t GetFileSize(const Path& path);

        static std::tuple<byte*, uint64_t> ReadFile(const Path& path);
        static bool ReadFile(const Path& path, void* buffer, int64_t size = -1);
        static String ReadTextFile(const Path& path);

        static bool WriteFile(const Path& path, byte* buffer, uint64_t size);

        static bool WriteTextFile(const Path& path, const String& text);
        static bool OpenFileDialog(FileDialogType type, const Path& initialDir, const String& filter,
                                   Vector<Path>& outpaths);
        static Ref<DataStream> OpenFile(const Path& filepath, bool readOnly = true);
    };
} // namespace Crowny
