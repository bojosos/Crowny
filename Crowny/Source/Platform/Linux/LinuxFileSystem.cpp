#include "cwpch.h"
#if 0
#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/PlatformUtils.h"

#include <GLFW/glfw3.h>

namespace Crowny
{

    bool FileSystem::FileExists(const Path& path)
    {
        std::ifstream f(path);
        return f.good();
    }

    int64_t FileSystem::GetFileSize(const Path& path)
    {
        std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
        return in.tellg();
    }

    std::tuple<uint8_t*, uint64_t> FileSystem::ReadFile(const Path& path)
    {
        std::ifstream input(path, std::ios::binary);

        Vector<uint8_t>* uint8_ts =
          new Vector<uint8_t>((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));

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

    Ref<DataStream> FileSystem::OpenFile(const Path& filepath, bool readOnly)
    {
        DataStream::AccessMode accessMode = DataStream::READ;
        if (!readOnly)
            accessMode = (DataStream::AccessMode)(accessMode | DataStream::WRITE);

        return CreateRef<FileDataStream>(filepath, accessMode, true);
    }

    Ref<DataStream> FileSystem::CreateAndOpenFile(const Path& filepath)
    {
        return CreateRef<FileDataStream>(filepath, DataStream::WRITE, true);
    }

    bool FileSystem::OpenFileDialog(FileDialogType type, const Path& initialDir, const String& filter,
                                    Vector<Path>& outPaths)
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
        execResult = execResult.erase(res.find_last_not_of(" \n\r\t") + 1);
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