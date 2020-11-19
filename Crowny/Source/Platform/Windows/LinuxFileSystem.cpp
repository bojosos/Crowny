#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Application/Application.h"

#include <fstream>
#include <GLFW/glfw3.h>

namespace Crowny
{

	static char* ReadCFile(FILE* f)
	{
		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		if (fsize == 0)
			return nullptr;

		fseek(f, 0, SEEK_SET);
		char* buf = new char[fsize + 1];
		fread(buf, 1, fsize, f);
		fclose(f);
		buf[fsize] = '\0';
	
		return buf;
	}

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

    	std::vector<byte> bytes(
         (std::istreambuf_iterator<char>(input)),
         (std::istreambuf_iterator<char>()));

    	input.close();
		return std::make_tuple(bytes.data(), bytes.size());
	}

	bool FileSystem::ReadFile(const std::string& path, void* buffer, int64_t size)
	{
		CW_ENGINE_CRITICAL("Read file void* not implemented");
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
	}

	bool FileSystem::WriteTextFile(const std::string& path, const std::string& text)
	{	
		std::ofstream fout;
		fout.open(path, std::ios::out);
		fout.write(text.c_str(), text.size());
		fout.close();
	}

	std::tuple<bool, std::string> FileSystem::OpenFileDialog(const char* filter, const std::string& initialDir, const std::string& title)
	{
		std::string cmd = "zenity --file-selection --filename=" + initialDir + " --title=" + title;
		FILE* f = popen(cmd.c_str(), "r");
		char* res = ReadCFile(f);
		if (!res)
			return std::make_tuple(false, std::string());

		return std::make_tuple(true, std::string(res));
	}

	std::tuple<bool, std::string> FileSystem::SaveFileDialog(const char* filter, const std::string& initialDir, const std::string& title)
	{
		std::string cmd = "zenity --file-selection --save --filename=" + initialDir + " --title=" + title;
		FILE* f = popen(cmd.c_str(), "r");
		char* res = ReadCFile(f);
		if (!res)
			return std::make_tuple(false, std::string());
		
		return std::make_tuple(true, std::string(res));
	}

}