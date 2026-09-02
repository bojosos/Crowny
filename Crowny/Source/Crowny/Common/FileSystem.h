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

    struct FileDialogOptions
    {
        FileDialogType Type = FileDialogType::OpenFile;
        String Title;
        Path InitialDirectory;
        Vector<DialogFilter> Filters;
        String DefaultName;
    };

    class FileSystem
    {
    public:
        static bool FileExists(const Path& path);
        static uint64_t GetFileSize(const Path& path);
        // Physical last-write time; packed built-in resources report an empty timestamp.
        static fs::file_time_type GetLastWriteTime(const Path& path);

        static std::tuple<byte*, uint64_t> ReadFile(const Path& path);
        static bool ReadFile(const Path& path, void* buffer, int64_t size = -1);
        static String ReadTextFile(const Path& path);

        static bool WriteFile(const Path& path, byte* buffer, uint64_t size);

        static bool WriteTextFile(const Path& path, const String& text);
        // Publishes a flushed sibling temporary file with one replace operation. A failure before replacement leaves the destination unchanged.
        static bool WriteFileAtomic(const Path& path, const byte* buffer, uint64_t size, String* outError = nullptr);
        static bool WriteTextFileAtomic(const Path& path, StringView text, String* outError = nullptr);
        static bool OpenFileDialog(FileDialogType type, Vector<Path>& outpaths, const String& title = {}, const Path& initialDir = {},
                                   const Vector<DialogFilter>& filters = {}, const String& filename = {});
        static bool OpenFileDialog(const FileDialogOptions& options, Vector<Path>& outPaths);
        static Vector<DialogFilter> ParseDialogFilters(StringView extensions);

        static Ref<DataStream> OpenFile(const Path& filepath, bool readOnly = true);
        static Ref<DataStream> CreateAndOpenFile(const Path& filepath);
    };
} // namespace Crowny
