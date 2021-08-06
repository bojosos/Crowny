#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

#include <GLFW/glfw3.h>
#include <fstream>

namespace Crowny
{
    static std::string FixPath(const std::string& badPath)
    {
        std::string res = badPath;
        size_t startPos = 0;
        while ((startPos = res.find("\\", startPos)) != std::string::npos)
        {
            res.replace(startPos, 1, "/");
            startPos++;
        }
        return res;
    }

    bool FileSystem::FileExists(const std::string& path)
    {
        FixPath(path);
        std::ifstream f(path);
        return f.good();
    }

    int64_t FileSystem::GetFileSize(const std::string& path)
    {
        FixPath(path);
        std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
        return in.tellg();
    }

    std::tuple<byte*, uint64_t> FileSystem::ReadFile(const std::string& path)
    {
        FixPath(path);
        std::ifstream input(path, std::ios::binary);

        std::vector<byte>* bytes =
          new std::vector<byte>((std::istreambuf_iterator<char>(input)), (std::istreambuf_iterator<char>()));

        input.close();
        return std::make_tuple(bytes->data(), bytes->size());
    }

    bool FileSystem::ReadFile(const std::string& path, void* buffer, int64_t size)
    {
        FixPath(path);
        CW_ENGINE_CRITICAL("Read file void* not implemented");
        return false;
    }

    std::string FileSystem::ReadTextFile(const std::string& path)
    {
        FixPath(path);
        std::ifstream input(path);

        std::string res((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        return res;
    }

    bool FileSystem::WriteFile(const std::string& path, byte* buffer, uint64_t size)
    {
        FixPath(path);
        std::ofstream fout;
        fout.open(path, std::ios::binary | std::ios::out);
        fout.write((char*)buffer, size);
        fout.close();
        return buffer != nullptr;
    }

    bool FileSystem::WriteTextFile(const std::string& path, const std::string& text)
    {
        FixPath(path);
        std::ofstream fout;
        fout.open(path, std::ios::out);
        fout.write(text.c_str(), text.size());
        fout.close();
        return !text.empty();
    }

    Ref<DataStream> FileSystem::OpenFile(const std::string& filepath, bool readOnly)
    {
        DataStream::AccessMode accessMode = DataStream::READ;
        if (!readOnly)
            accessMode = (DataStream::AccessMode)(accessMode | DataStream::WRITE);

        return CreateRef<FileDataStream>(filepath, accessMode, true);
    }

    bool FileSystem::OpenFileDialog(FileDialogType type, const std::string& initialDir, const std::string& filter,
                                    std::vector<std::string>& outPaths)
    {
        FixPath(initialDir);
        std::string add;
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

        std::string cmd = "zenity --file-selection --filename=\"" + initialDir + "\"" + add;
        FILE* f = popen(cmd.c_str(), "r");
        if (!f)
            return false;

        std::array<char, 128> buffer;
        std::string res = "";
        while (fgets(buffer.data(), 128, f))
        {
            res += buffer.data();
        }

        if (res.empty())
            return false;

        res = res.erase(res.find_last_not_of(" \n\r\t") + 1);
        outPaths = StringUtils::SplitString(res, "|");

        return true;
    }

} // namespace Crowny
