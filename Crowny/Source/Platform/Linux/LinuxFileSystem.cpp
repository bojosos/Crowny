#include "cwpch.h"
#ifdef CW_PLATFORM_LINUX
#include "Crowny/Application/Application.h"
#include "Crowny/Common/BuiltInResourcePack.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <GLFW/glfw3.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

namespace Crowny
{
    namespace
    {
        bool ReportAtomicWriteError(String* outError, StringView operation, int code)
        {
            if (outError != nullptr)
                *outError = String(operation) + " failed: " + std::strerror(code);
            return false;
        }

        Path MakeAtomicTemporaryPath(const Path& path)
        {
            static std::atomic<uint64_t> sequence = 0;
            const String filename = "." + path.filename().string() + ".tmp." + std::to_string(getpid()) + "." +
                                    std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            return path.parent_path() / filename;
        }
    } // namespace

    static Ref<DataStream> OpenPackedFile(const Path& path)
    {
        return BuiltInResourcePack::IsStartedUp() ? BuiltInResourcePack::Get().Open(path) : nullptr;
    }

    bool FileSystem::FileExists(const Path& path)
    {
        if (BuiltInResourcePack::IsStartedUp() && BuiltInResourcePack::Get().Contains(path))
            return true;
        std::ifstream f(path);
        return f.good();
    }

    uint64_t FileSystem::GetFileSize(const Path& path)
    {
        uint64_t packedSize = 0;
        if (BuiltInResourcePack::IsStartedUp() && BuiltInResourcePack::Get().TryGetSize(path, packedSize))
            return packedSize;
        std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
        return in ? static_cast<uint64_t>(in.tellg()) : 0;
    }

    std::tuple<uint8_t*, uint64_t> FileSystem::ReadFile(const Path& path)
    {
        if (const Ref<DataStream> packed = OpenPackedFile(path))
        {
            uint8_t* data = new uint8_t[packed->Size()];
            const uint64_t size = packed->Read(data, packed->Size());
            return std::make_tuple(data, size);
        }
        std::ifstream input(path, std::ios::binary);

        Vector<uint8_t>* uint8_ts = new Vector<uint8_t>((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));

        input.close();
        return std::make_tuple(uint8_ts->data(), uint8_ts->size());
    }

    bool FileSystem::ReadFile(const Path& path, void* buffer, int64_t size)
    {
        CW_ENGINE_CRITICAL("Read file void* not implemented");
        return false;
    }

    String FileSystem::ReadTextFile(const Path& path)
    {
        if (const Ref<DataStream> packed = OpenPackedFile(path))
            return packed->GetAsString();
        std::ifstream input(path);

        String res((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        return res;
    }

    bool FileSystem::WriteFile(const Path& path, uint8_t* buffer, uint64_t size)
    {
        std::ofstream fout;
        fout.open(path, std::ios::binary | std::ios::out);
        fout.write((char*)buffer, size);
        fout.close();
        return buffer != nullptr;
    }

    bool FileSystem::WriteTextFile(const Path& path, const String& text)
    {
        std::ofstream fout;
        fout.open(path, std::ios::out);
        fout.write(text.c_str(), text.size());
        fout.close();
        return !text.empty();
    }

    bool FileSystem::WriteFileAtomic(const Path& path, const byte* buffer, uint64_t size, String* outError)
    {
        if (outError != nullptr)
            outError->clear();
        if (path.empty() || (buffer == nullptr && size != 0))
        {
            if (outError != nullptr)
                *outError = "Atomic write requires a destination and non-null data for a non-empty payload.";
            return false;
        }

        std::error_code directoryError;
        const Path parent = path.parent_path();
        if (!parent.empty())
            fs::create_directories(parent, directoryError);
        if (directoryError)
        {
            if (outError != nullptr)
                *outError = "Failed to create the destination directory: " + directoryError.message();
            return false;
        }

        Path temporary;
        int file = -1;
        for (uint32_t attempt = 0; attempt < 32 && file < 0; attempt++)
        {
            temporary = MakeAtomicTemporaryPath(path);
            file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
            if (file < 0 && errno != EEXIST)
                return ReportAtomicWriteError(outError, "Creating the atomic-write temporary file", errno);
        }
        if (file < 0)
        {
            if (outError != nullptr)
                *outError = "Could not allocate a unique atomic-write temporary file.";
            return false;
        }

        bool success = true;
        uint64_t written = 0;
        while (written < size)
        {
            const size_t chunk = static_cast<size_t>(std::min<uint64_t>(size - written, std::numeric_limits<ssize_t>::max()));
            const ssize_t chunkWritten = write(file, buffer + written, chunk);
            if (chunkWritten < 0 && errno == EINTR)
                continue;
            if (chunkWritten <= 0)
            {
                success = ReportAtomicWriteError(outError, "Writing the atomic-write temporary file", errno);
                break;
            }
            written += static_cast<uint64_t>(chunkWritten);
        }
        if (success && fsync(file) != 0)
            success = ReportAtomicWriteError(outError, "Flushing the atomic-write temporary file", errno);
        if (close(file) != 0 && success)
            success = ReportAtomicWriteError(outError, "Closing the atomic-write temporary file", errno);

        if (success && ::rename(temporary.c_str(), path.c_str()) != 0)
            success = ReportAtomicWriteError(outError, "Publishing the atomic-write temporary file", errno);
        if (success)
        {
            const Path directory = parent.empty() ? Path(".") : parent;
            const int directoryFile = open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (directoryFile >= 0)
            {
                fsync(directoryFile);
                close(directoryFile);
            }
        }
        else
        {
            std::error_code cleanupError;
            fs::remove(temporary, cleanupError);
        }
        return success;
    }

    bool FileSystem::WriteTextFileAtomic(const Path& path, StringView text, String* outError)
    {
        return WriteFileAtomic(path, reinterpret_cast<const byte*>(text.data()), text.size(), outError);
    }

    Ref<DataStream> FileSystem::OpenFile(const Path& filepath, bool readOnly)
    {
        if (readOnly)
        {
            if (Ref<DataStream> packed = OpenPackedFile(filepath))
                return packed;
        }
        DataStream::AccessMode accessMode = DataStream::READ;
        if (!readOnly)
            accessMode = (DataStream::AccessMode)(accessMode | DataStream::WRITE);

        return CreateRef<FileDataStream>(filepath, accessMode, true);
    }

    Ref<DataStream> FileSystem::CreateAndOpenFile(const Path& filepath) { return CreateRef<FileDataStream>(filepath, DataStream::WRITE, true); }

    bool FileSystem::OpenFileDialog(FileDialogType type, Vector<Path>& outPaths, const String& title, const Path& initialDir,
                                    const Vector<DialogFilter>& filter, const String& filename)
    {
        String add;
        // TODO: Check if all of these work, make it more configurable
        switch (type)
        {
        case FileDialogType::OpenFile:
            add = "title=\"Open file\"";
            break;
        case FileDialogType::SaveFile:
            add = "title=\"Save file\" --save";
            break;
        case FileDialogType::Multiselect:
            add = " --multiple title=\"Open files\"";
            break;
        case FileDialogType::OpenFolder:
            add = " --directory title=\"Open folder\"";
            break;
        }

        String execResult = PlatformUtils::Exec("zenity --file-selection --filename=\"" + initialDir.string() + "\"" + add);
        execResult = execResult.erase(execResult.find_last_not_of(" \n\r\t") + 1);
        for (const String& str : StringUtils::SplitString(execResult, "|"))
            outPaths.push_back(Path(std::move(str)));

        return true;
    }

    static bool ZenityCheck()
    {
        FILE* fp;
        const uint32_t ZENITY_MAX_PATH = 512;
        char path[ZENITY_MAX_PATH];
        fp = popen("which zenity", "r");
        if (fp == NULL)
            CW_ENGINE_ERROR("Zenity check: null file ptr.");
        if (fgets(path, ZENITY_MAX_PATH, fp) == NULL)
        {
            CW_ENGINE_ERROR("Zenity not found in path. You will not be able to open file browse dialogs");
            pclose(fp);
            return false;
        }
        pclose(fp);
        return true;
    }

} // namespace Crowny
#endif
