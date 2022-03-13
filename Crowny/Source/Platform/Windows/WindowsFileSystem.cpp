#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common//StringUtils.h"
#include "Crowny/Common/UTF8.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#include <ShlObj_core.h>
#undef GLFW_EXPOSE_NATIVE_WIN32

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

	void AddFilters(IFileDialog* fileDialog, const Vector<DialogFilter>& filters)
	{
		if (filters.size() == 0)
			return;

		COMDLG_FILTERSPEC* specList = new COMDLG_FILTERSPEC[filters.size()];
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

	bool FileSystem::OpenFileDialog(FileDialogType type, const Path& initialDir, const Vector<DialogFilter>& filter, Vector<Path>& outPaths)
	{
		CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		IFileDialog* fileDialog = nullptr;
		bool isOpenDialog = type == FileDialogType::OpenFile || type == FileDialogType::OpenFolder || type == FileDialogType::Multiselect;
		IID classId = isOpenDialog ? CLSID_FileOpenDialog : CLSID_FileSaveDialog;
		CoCreateInstance(classId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileDialog));

		AddFilters(fileDialog, filter);
		SetInitialDir(fileDialog, initialDir);

		bool isMultiselected = type == FileDialogType::Multiselect;
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

	//bool FileSystem::OpenFileDialog(FileDialogType type, const Path& initialDir, const String& filter, Vector<Path>& outPaths)
	//{
	//	OPENFILENAME ofn = { 0 };
	//	TCHAR szFile[260] = { 0 };
	//
	//	char* title = nullptr;
	//	switch (type)
	//	{
	//	case FileDialogType::OpenFile:
	//		title = "Open file";
	//		break;
	//	case FileDialogType::SaveFile:
	//		title = "Save file";
	//		break;
	//	case FileDialogType::Multiselect:
	//		title = "Open files";
	//		break;
	//	case FileDialogType::OpenFolder:
	//		title = "Open folder";
	//		break;
	//	}

	//	ofn.lStructSize = sizeof(ofn);
	//	ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
	//	ofn.lpstrFile = szFile;
	//	ofn.nMaxFile = sizeof(szFile);
	//	ofn.lpstrTitle = title;
	//	ofn.lpstrFilter = filter.c_str();
	//	ofn.nFilterIndex = 1;
	//	ofn.lpstrFileTitle = NULL;
	//	ofn.nMaxFileTitle = 0; // TODO: maybe use cur dir here if empty?
	//	ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.string().c_str();
	//	ofn.Flags = /*OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | */ OFN_EXPLORER | OFN_ENABLESIZING;

	//	if (type == FileDialogType::OpenFile)
	//	{
	//		if (GetOpenFileName(&ofn) == TRUE)
	//		{
	//			outPaths.push_back(Path(ofn.lpstrFile));
	//			return true;
	//		}
	//	}

	//	if (type == FileDialogType::SaveFile)
	//	{
	//		if (GetSaveFileName(&ofn) == TRUE)
	//		{
	//			outPaths.push_back(Path(ofn.lpstrFile));
	//			return true;
	//		}
	//	}

	//	if (type == FileDialogType::Multiselect)
	//	{
	//		ofn.Flags |= OFN_ALLOWMULTISELECT;
	//		if (GetOpenFileName(&ofn) == TRUE)
	//		{
	//			// TODO: fix this
	//			// outPaths.push_back(Path(ofn.lpstrFile)); null separated and another null after?
	//			CW_ENGINE_INFO(ofn.lpstrFile);
	//			return true;
	//		}
	//	}

	//	if (type == FileDialogType::OpenFolder)
	//	{
	//		std::string path;
	//		path.resize(256);
	//		BROWSEINFO browseinfo = { 0 };
	//		browseinfo.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());;
	//		browseinfo.lpszTitle = title;
	//		browseinfo.ulFlags = BIF_NEWDIALOGSTYLE /* | BIF_RETURNONLYFSDIRS 0*/ ;
	//		LPITEMIDLIST pidl = SHBrowseForFolder(&browseinfo);
	//		if (pidl != nullptr)
	//		{
	//			SHGetPathFromIDList(pidl, path.data());
	//			IMalloc* imalloc = nullptr;
	//			if (SUCCEEDED(SHGetMalloc(&imalloc)))
	//			{
	//				imalloc->Free(pidl);
	//				imalloc->Release();
	//			}
	//		}
	//		outPaths.push_back(path);
	//	}

	//	return false;
	//}

}