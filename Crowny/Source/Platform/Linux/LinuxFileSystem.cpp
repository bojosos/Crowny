#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Common/Parser.h"

#include <fstream>
#include <GLFW/glfw3.h>

namespace Crowny
{

	bool FileSystem::FileExists(const std::string& path)
	{
		std::ifstream f(path);
    	return f.good();
	}

	int64_t FileSystem::GetFileSize(const std::string& path)
	{
		std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
		return in.tellg(); 
	}

	std::tuple<byte*, uint64_t> FileSystem::ReadFile(const std::string& path)
	{
		std::ifstream input(path, std::ios::binary);
		
    	std::vector<byte>* bytes = new std::vector<byte>(
         (std::istreambuf_iterator<char>(input)),
         (std::istreambuf_iterator<char>()));

    	input.close();
		return std::make_tuple(bytes->data(), bytes->size());
	}

	bool FileSystem::ReadFile(const std::string& path, void* buffer, int64_t size)
	{
		CW_ENGINE_CRITICAL("Read file void* not implemented");
		return false;
	}

	std::string FileSystem::ReadTextFile(const std::string& path)
	{
		std::ifstream input(path);
		
		std::string res((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		input.close();
		return res;
	}

	bool FileSystem::WriteFile(const std::string& path, byte* buffer)
	{
		std::ofstream fout;
		fout.open(path, std::ios::binary | std::ios::out);
		fout.write((char*)buffer, sizeof(buffer));
		fout.close();
		return buffer != nullptr;
	}

	bool FileSystem::WriteTextFile(const std::string& path, const std::string& text)
	{	
		std::ofstream fout;
		fout.open(path, std::ios::out);
		fout.write(text.c_str(), text.size());
		fout.close();
		return !text.empty();
	}

	bool FileSystem::OpenFileDialog(FileDialogType type, const std::string& initialDir, const std::string& filter, std::vector<std::string>& outPaths)
	{	
		const char* save;
		const char* multiple;
		const char* title = "Open file";

		if (type == FileDialogType::OpenFile)
		{
			title = "Open file";
		}

		if (type == FileDialogType::SaveFile)
		{
			save = " --save";
			title = "Save file";
		}

		if (type == FileDialogType::Multiselect)
		{
			multiple = " --multiple";
			title = "Open files";
		}

		std::string cmd = "zenity --file-selection --filename=\"" + initialDir + "\" --title=\"" + title + "\"" + save + multiple;
		FILE* f = popen(cmd.c_str(), "r");
		if (!f)
			return false;

		std::array<char, 128> buffer;
		std::string res = "";
		while(fgets(buffer.data(), 128, f))
		{
			res += buffer.data();
		}

		if (res.empty()) 
			return false;

		res = res.erase(res.find_last_not_of(" \n\r\t") + 1);
		outPaths = SplitString(res, "|");
		
		return true;
	}

}