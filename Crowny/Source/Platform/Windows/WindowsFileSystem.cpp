#if 0
#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#undef GLFW_EXPOSE_NATIVE_WIN32

namespace Crowny
{
	//TODO: Make all of these work with Unicode
	void CALLBACK FileIOCompleted(DWORD dwErrorCode, DWORD dwBytesTransfered, LPOVERLAPPED ol)
	{

	}

	static HANDLE OpenFileForReadingWin32(const String& path)
	{
		return CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	}

	static bool ReadFileWin32(HANDLE file, void* buffer, int64_t size)
	{
		OVERLAPPED ol = { 0 };
		return ReadFileEx(file, buffer, size, &ol, FileIOCompleted);
	}

	static int64_t GetFileSizeWin32(HANDLE file)
	{
		LARGE_INTEGER size;
		GetFileSizeEx(file, &size);
		return size.QuadPart;
	}

	bool FileSystem::FileExists(const String& path)
	{
		DWORD res = GetFileAttributes(path.c_str()); // is this safe?
		return !(res == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND);
	}

	int64_t FileSystem::GetFileSize(const String& path)
	{
		HANDLE file = OpenFileForReadingWin32(path);
		if (file == INVALID_HANDLE_VALUE)
			return -1;
		int64_t size = GetFileSizeWin32(file);
		CloseHandle(file);
		return size;
	}

	std::tuple<byte*, uint64_t> FileSystem::ReadFile(const String& path)
	{
		HANDLE file = OpenFileForReadingWin32(path);

		int64_t size = GetFileSizeWin32(file);

		byte* buff = new byte[size];
		bool success = ReadFileWin32(file, buff, size);

		CloseHandle(file);

		if (!success)
			delete[] buff;

		return success ? std::make_tuple(buff, size) : std::make_tuple(nullptr, -1);
	}

	bool FileSystem::ReadFile(const String& path, void* buffer, int64_t size)
	{
		HANDLE file = OpenFileForReadingWin32(path);
		CW_ENGINE_ASSERT(file != INVALID_HANDLE_VALUE, path);

		if (size < 0)
		{
			size = GetFileSizeWin32(file);
		}

		bool success = ReadFileWin32(file, buffer, size);
		CloseHandle(file);
		return success;
	}

	String FileSystem::ReadTextFile(const String& path)
	{
		HANDLE file = OpenFileForReadingWin32(path);
		CW_ENGINE_ASSERT(file != INVALID_HANDLE_VALUE, path);

		int64_t size = GetFileSizeWin32(file);
		String res(size, 0);
		bool success = ReadFileWin32(file, &res[0], size);
		CloseHandle(file);

		return success ? res : String();
	}

	// TODO: investigate this, or just use c++ api
	bool FileSystem::WriteFile(const String& path, byte* buffer, uint64_t sz)
	{
		HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, NULL, NULL, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		CW_ENGINE_ASSERT(file != INVALID_HANDLE_VALUE, path);

		int64_t size = GetFileSizeWin32(file);
		DWORD written;
		bool success = ::WriteFile(file, buffer, size, &written, nullptr);
		CloseHandle(file);

		return success;
	}

	bool FileSystem::WriteTextFile(const String& path, const String& text)
	{
		return WriteFile(path, (byte*)text[0]);
	}

	bool FileSystem::OpenFileDialog(FileDialogType type, const String& initialDir = "", const String& filter, Vector<String>& outpaths)
	{
		OPENFILENAME ofn = { 0 };
		TCHAR szFile[260] = { 0 };
		
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrTitle = title.c_str();
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (type == FileDialogType::OpenFile)
		{
			if (GetOpenFileName(&ofn) == TRUE)
			{
				return std::make_tuple(true, ofn.lpstrFile);
			}
		}

		if (type == FileDialogType::SaveFile)
		{
			if (GetSaveFileName(&ofn) == TRUE)
			{
				return std::make_tuple(true, ofn.lpstrFile);
			}
		}

		if (type == FileDialogType::Multiselect)
		{
			ofn.Flags |= OFN_ALLOWMULTISELECT;
			if (GetOpenFileName(&ofn) == TRUE)
			{
				return std::make_tuple(true, ofn.lpstrFile);
			}
		}

		return std::make_tuple(false, "");
	}

}
#endif