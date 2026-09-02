#include "cwpch.h"

#ifdef CW_PLATFORM_WIN32
#include "Crowny/Application/Application.h"
#include "Crowny/Common//StringUtils.h"
#include "Crowny/Common/BuiltInResourcePack.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/UTF8.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <ShlObj_core.h>
#include <atomic>
#include <commdlg.h>
#include <limits>
#undef GLFW_EXPOSE_NATIVE_WIN32

namespace Crowny
{
    namespace
    {
        bool ReportAtomicWriteError(String* outError, StringView operation, DWORD code)
        {
            if (outError != nullptr)
                *outError = String(operation) + " failed with Windows error " + std::to_string(code) + ".";
            return false;
        }

        Path MakeAtomicTemporaryPath(const Path& path)
        {
            static std::atomic<uint64_t> sequence = 0;
            const String filename = "." + path.filename().string() + ".tmp." + std::to_string(GetCurrentProcessId()) + "." +
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
        return fs::exists(path) || (BuiltInResourcePack::IsStartedUp() && BuiltInResourcePack::Get().Contains(path));
    }

    uint64_t FileSystem::GetFileSize(const Path& path)
    {
        uint64_t packedSize = 0;
        if (BuiltInResourcePack::IsStartedUp() && BuiltInResourcePack::Get().TryGetSize(path, packedSize))
            return packedSize;
        std::error_code error;
        const uint64_t size = fs::file_size(path, error);
        return error ? 0 : size;
    }

    fs::file_time_type FileSystem::GetLastWriteTime(const Path& path)
    {
        std::error_code error;
        const fs::file_time_type writeTime = fs::last_write_time(path, error);
        return error ? fs::file_time_type{} : writeTime;
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
        HANDLE file = INVALID_HANDLE_VALUE;
        for (uint32_t attempt = 0; attempt < 32 && file == INVALID_HANDLE_VALUE; attempt++)
        {
            temporary = MakeAtomicTemporaryPath(path);
            file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS)
                return ReportAtomicWriteError(outError, "Creating the atomic-write temporary file", GetLastError());
        }
        if (file == INVALID_HANDLE_VALUE)
        {
            if (outError != nullptr)
                *outError = "Could not allocate a unique atomic-write temporary file.";
            return false;
        }

        bool success = true;
        uint64_t written = 0;
        while (written < size)
        {
            const DWORD chunk = static_cast<DWORD>(std::min<uint64_t>(size - written, std::numeric_limits<DWORD>::max()));
            DWORD chunkWritten = 0;
            const BOOL writeSucceeded = ::WriteFile(file, buffer + written, chunk, &chunkWritten, nullptr);
            if (!writeSucceeded || chunkWritten != chunk)
            {
                const DWORD writeError = writeSucceeded ? ERROR_WRITE_FAULT : GetLastError();
                success = ReportAtomicWriteError(outError, "Writing the atomic-write temporary file", writeError);
                break;
            }
            written += chunkWritten;
        }
        if (success && !FlushFileBuffers(file))
            success = ReportAtomicWriteError(outError, "Flushing the atomic-write temporary file", GetLastError());
        CloseHandle(file);

        if (success && !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            success = ReportAtomicWriteError(outError, "Publishing the atomic-write temporary file", GetLastError());
        if (!success)
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

    void AddFilters(IFileDialog* fileDialog, const Vector<DialogFilter>& filters)
    {
        if (filters.size() == 0)
            return;

        COMDLG_FILTERSPEC* specList = new COMDLG_FILTERSPEC[(uint32_t)filters.size()];
        for (uint32_t i = 0; i < (uint32_t)filters.size(); i++)
        {
            std::wstring str1 = UTF8::ToWide(filters[i].Name);
            std::wstring str2 = UTF8::ToWide(filters[i].FilterSpec);
            wchar_t* name = new wchar_t[str1.size() + 1];
            name[str1.size()] = L'\0';
            wchar_t* spec = new wchar_t[str2.size() + 1];
            spec[str2.size()] = L'\0';
            std::memcpy((void*)name, str1.c_str(), str1.size() * sizeof(wchar_t)); // leak?
            std::memcpy((void*)spec, str2.c_str(), str2.size() * sizeof(wchar_t));
            specList[i].pszName = name;
            specList[i].pszSpec = spec;
        }
        fileDialog->SetFileTypes((uint32_t)filters.size(), specList);
        for (uint32_t i = 0; i < (uint32_t)filters.size(); i++)
        {
            delete[] specList[i].pszName;
            delete[] specList[i].pszSpec;
        }
        delete[] specList;
    }

    void SetInitialDir(IFileDialog* fileDialog, const Path& initialDir)
    {
        const wchar_t* pathStr = initialDir.c_str();
        IShellItem* folder;
        HRESULT result = SHCreateItemFromParsingName(pathStr, NULL, IID_PPV_ARGS(&folder));
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE))
            return;
        if (!SUCCEEDED(result))
            return;
        fileDialog->SetFolder(folder);
        folder->Release();
    }

    void GetPaths(IShellItemArray* shellItems, Vector<Path>& outPaths)
    {
        DWORD numItems;
        shellItems->GetCount(&numItems);
        for (DWORD i = 0; i < numItems; i++)
        {
            IShellItem* shellItem = nullptr;
            shellItems->GetItemAt(i, &shellItem);
            SFGAOF attribs;
            shellItem->GetAttributes(SFGAO_FILESYSTEM, &attribs);

            if (!(attribs & SFGAO_FILESYSTEM))
                continue;
            LPWSTR name;
            shellItem->GetDisplayName(SIGDN_FILESYSPATH, &name);
            outPaths.push_back(Path(name));
            CoTaskMemFree(name);
        }
    }

    bool FileSystem::OpenFileDialog(FileDialogType type, Vector<Path>& outPaths, const String& title, const Path& initialDir,
                                    const Vector<DialogFilter>& filter, const String& filename)
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        IFileDialog* fileDialog = nullptr;
        bool isOpenDialog = type == FileDialogType::OpenFile || type == FileDialogType::OpenFolder || type == FileDialogType::Multiselect;
        IID classId = isOpenDialog ? CLSID_FileOpenDialog : CLSID_FileSaveDialog;
        CoCreateInstance(classId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileDialog));

        AddFilters(fileDialog, filter);
        SetInitialDir(fileDialog, initialDir);
        String titleString = title;
        if (titleString.empty())
        {
            switch (type)
            {
            case FileDialogType::OpenFile:
                titleString = "Open File";
                break;
            case FileDialogType::OpenFolder:
                titleString = "Open Folder";
                break;
            case FileDialogType::SaveFile:
                titleString = "Save File";
                break;
            }
        }
        fileDialog->SetTitle(UTF8::ToWide(titleString).c_str());
        fileDialog->SetFileName(UTF8::ToWide(filename).c_str());

        const bool isMultiselected = type == FileDialogType::Multiselect;
        if (isOpenDialog)
        {
            if (type == FileDialogType::OpenFolder)
            {
                DWORD dwFlags;
                fileDialog->GetOptions(&dwFlags);
                fileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS);
            }
            else if (type == FileDialogType::Multiselect)
            {
                DWORD dwFlags;
                fileDialog->GetOptions(&dwFlags);
                fileDialog->SetOptions(dwFlags | FOS_ALLOWMULTISELECT);
            }
        }

        bool finalResult = false;
        if (SUCCEEDED(fileDialog->Show(nullptr)))
        {
            if (type == FileDialogType::Multiselect)
            {
                IFileOpenDialog* fileOpenDialog;
                fileDialog->QueryInterface(IID_IFileOpenDialog, (void**)&fileOpenDialog);
                IShellItemArray* shellItems = nullptr;
                fileOpenDialog->GetResults(&shellItems);
                GetPaths(shellItems, outPaths);
                shellItems->Release();
                fileOpenDialog->Release();
            }
            else
            {
                IShellItem* shellItem = nullptr;
                fileDialog->GetResult(&shellItem);
                LPWSTR filePath = nullptr;
                shellItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
                outPaths.push_back(Path(filePath));
                CoTaskMemFree(filePath);
                shellItem->Release();
            }
            finalResult = true;
        }

        CoUninitialize();
        return finalResult;
    }

} // namespace Crowny
#endif
