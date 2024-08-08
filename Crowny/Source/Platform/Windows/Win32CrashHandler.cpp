#include "cwpch.h"

#include "Crowny/Application/CrashHandler.h"
#include "Crowny/Common/PlatformUtils.h"

#include <DbgHelp.h>
#include <Windows.h>
#include <psapi.h>

static const char* s_MiniDumpName = "minidump.dmp";
static const char* s_CrashDirectory = "bugreports";
static const char* s_CrashTraceName = "bugreport.txt";
static const wchar_t* s_FatalErrorMessage = L"Crash... It is what it is...";

#define MAX_STACKTRACE_NAME 2048
#define MAX_STACKTRACE_DEPTH 256U

namespace Crowny
{
    struct Data
    {
        Crowny::Mutex Mutex;
    };

    static Data* s_Data;

    CrashHandler::CrashHandler() { s_Data = new Data(); }

    CrashHandler::~CrashHandler() { delete s_Data; }

    void Win32LoadSymbols()
    {
        HANDLE hProcess = GetCurrentProcess();
        uint32_t options = SymGetOptions();
        options |=
            SYMOPT_LOAD_LINES | SYMOPT_EXACT_SYMBOLS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS;
        SymSetOptions(options);
        if (!SymInitialize(hProcess, nullptr, false))
        {
            CW_ENGINE_ERROR("SymInitialize failed");
            return;
        }

        DWORD bufferSize;
        EnumProcessModules(hProcess, nullptr, 0, &bufferSize);

        uint32_t numModules = bufferSize / sizeof(HMODULE);
        HMODULE* modules = new HMODULE[numModules];
        EnumProcessModules(hProcess, modules, bufferSize, &bufferSize);

        for (uint32_t i = 0; i < numModules; i++)
        {
            MODULEINFO moduleInfo;
            char moduleName[MAX_STACKTRACE_NAME];
            char imageName[MAX_STACKTRACE_NAME];

            GetModuleInformation(hProcess, modules[i], &moduleInfo, sizeof(moduleInfo));
            GetModuleFileNameEx(hProcess, modules[i], imageName, MAX_STACKTRACE_NAME);
            GetModuleBaseName(hProcess, modules[i], moduleName, MAX_STACKTRACE_NAME);

            char pdbSearchPath[MAX_STACKTRACE_NAME];
            char* filename = nullptr;
            GetFullPathNameA(moduleName, MAX_STACKTRACE_NAME, pdbSearchPath, &filename);
            *filename = '\0';
            SymSetSearchPath(GetCurrentProcess(), pdbSearchPath);
            DWORD64 moduleAddress = SymLoadModule64(hProcess, modules[i], imageName, moduleName,
                                                    (DWORD64)moduleInfo.lpBaseOfDll, (DWORD)moduleInfo.SizeOfImage);
            if (moduleAddress)
            {
                IMAGEHLP_MODULE64 imageInfo;
                std::memset(&imageInfo, 0, sizeof(IMAGEHLP_MODULE64));
                imageInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
                if (!SymGetModuleInfo64(GetCurrentProcess(), moduleAddress, &imageInfo))
                {
                    CW_ENGINE_WARN("Bad retrieve for module info");
                }
                else
                {
                    // if (imageInfo.SymType == SymNone)
                    //     CW_ENGINE_INFO("Failed loading symbols for module: {0}", moduleName);
                }
            }
            else
            {
                CW_ENGINE_INFO("Failed loading module {0}. Error: {1}, Path: {2}, Image: {3}", moduleName, imageName);
            }
        }
        delete[] modules;
        // gSymbolsLoaded = true;
    }

    String Win32GetExceptionMessage(EXCEPTION_RECORD* record)
    {
        String format;
        switch (record->ExceptionCode)
        {
        case EXCEPTION_ACCESS_VIOLATION: {
            DWORD_PTR address = 0;
            format = fmt::format("Unhandeled exception at 0x{0:p}", record->ExceptionAddress);
            if (record->NumberParameters == 2)
            {
                if (record->ExceptionInformation[0] == 0)
                    format = fmt::format("Unhandeled exception at {0:p}, access violation reading 0x{1:x}.",
                                         record->ExceptionAddress, record->ExceptionInformation[1]);
                else if (record->ExceptionInformation[0] == 1)
                    format = fmt::format("Unhandeled exception at {0:p}, access violation writing 0x{1:x}.",
                                         record->ExceptionAddress, record->ExceptionInformation[1]);
                else if (record->ExceptionInformation[0] == 8)
                    format = fmt::format("Unhandeled exception at {0:p}, access violation DEP 0x{1:x}.",
                                         record->ExceptionAddress, record->ExceptionInformation[1]);
            }
            break;
        }
        case EXCEPTION_IN_PAGE_ERROR: {
            DWORD_PTR violatedAddress = 0;
            DWORD_PTR code = 0;
            if (record->NumberParameters == 3)
            {
                // TODO: Format message for record->ExceptionInformation[0][2](NTSTATUS)
                if (record->ExceptionInformation[0] == 0)
                    format = fmt::format(
                      "Unhandeled exception at {0:p}, page fault reading {0:p} with code {0:p}",
                      record->ExceptionAddress, record->ExceptionInformation[1], record->ExceptionInformation[2]);
                else if (record->ExceptionInformation[0] == 1)
                    format = fmt::format(
                      "Unhandeled exception at {0:p}, page fault writing {0:p} with code {0:p}",
                      record->ExceptionAddress, record->ExceptionInformation[1], record->ExceptionInformation[2]);
                else if (record->ExceptionInformation[0] == 8)
                    format = fmt::format("Unhandeled exception at {0:p}, page fault DEP {0:p} with code {0:p}",
                                         record->ExceptionAddress, record->ExceptionInformation[1],
                                         record->ExceptionInformation[2]);
            }
            break;
        }
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            format = fmt::format("Unhandeled exception at 0x{0:x}, array out of bounds", record->ExceptionAddress);
            break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            format = fmt::format("Unhandeled exception at 0x{0:x}, misaligned data", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float denormal operand", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float divide by zero", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float invalid oepration", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_OVERFLOW:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float overflow", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float underflow", record->ExceptionAddress);
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float stack overflow/underflow", record->ExceptionAddress);
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            format =
              fmt::format("Unhandeled exception at 0x{0:x}, illegal instruction", record->ExceptionAddress);
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            format = fmt::format("Unhandeled exception at 0x{0:x}, executing prviate instruction", record->ExceptionAddress);
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float divide by zero", record->ExceptionAddress);
            break;
        case EXCEPTION_INT_OVERFLOW:
            format = fmt::format("Unhandeled exception at 0x{0:x}, float overflow", record->ExceptionAddress);
            break;
        case EXCEPTION_STACK_OVERFLOW:
            format = fmt::format("Unhandeled exception at 0x{0:x}, stack overflow go brrrr", record->ExceptionAddress);
            break;
        case EXCEPTION_GUARD_PAGE:
            format = fmt::format("Unhandeled exception at 0x{0:x}, guard page", record->ExceptionAddress);
            break;
        default:
            format =
              fmt::format("Unhandeled exception at {0:p}. Code: 0x{1:x}", record->ExceptionAddress, record->ExceptionCode);
            break;
        }
        return format;
    }

    struct MiniDumpData
    {
        Path FilePath;
        EXCEPTION_POINTERS* ExceptionData;
        DWORD ThreadId;
    };

    BOOL CALLBACK MyMiniDumpCallback(PVOID pParam, const PMINIDUMP_CALLBACK_INPUT pInput,
                                     PMINIDUMP_CALLBACK_OUTPUT pOutput)
    {
        if (pInput == 0)
            return false;

        if (pOutput == 0)
            return false;

        switch (pInput->CallbackType)
        {
        case IncludeModuleCallback:
            return true;
        case IncludeThreadCallback:
            return true;
        case ModuleCallback:
            if (!(pOutput->ModuleWriteFlags & ModuleReferencedByMemory))
                pOutput->ModuleWriteFlags &= (~ModuleWriteModule);
            return true;
        case ThreadCallback:
            return true;
        case ThreadExCallback:
            return true;
        case MemoryCallback:
            return false;
        case CancelCallback:
            return false;
        default:
            return false;
        }
    }

    DWORD CALLBACK WriteMiniDumpUtil(void* data)
    {
        MiniDumpData* params = (MiniDumpData*)data;
        HANDLE hFile =
          CreateFileW(params->FilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = params->ThreadId;
            mei.ExceptionPointers = params->ExceptionData;
            mei.ClientPointers = false;
            MINIDUMP_CALLBACK_INFORMATION mci;
            mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)MyMiniDumpCallback;
            mci.CallbackParam = 0;
            MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory, MiniDumpScanMemory);
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, &mei, nullptr, &mci);
            CloseHandle(hFile);
        }
        return 0;
    }

    void WriteMiniDump(const Path& path, EXCEPTION_POINTERS* exceptionData)
    {
        DWORD threadId = 0;
        MiniDumpData params = { path, exceptionData, GetCurrentThreadId() };
        HANDLE hThread = CreateThread(nullptr, 0, &WriteMiniDumpUtil, &params, 0, &threadId);
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    uint32_t GetRawStackTrace(CONTEXT* context, uint64_t stackTrace[MAX_STACKTRACE_DEPTH])
    {
        HANDLE hProcess = GetCurrentProcess();
        HANDLE hThread = GetCurrentThread();
        uint32_t machineType;
        STACKFRAME64 stackFrame;
        std::memset(&stackFrame, 0, sizeof(STACKFRAME64));

        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
#if defined(_M_X64)
        stackFrame.AddrPC.Offset = context->Rip;
        stackFrame.AddrStack.Offset = context->Rsp;
        stackFrame.AddrFrame.Offset = context->Rbp;
        machineType = IMAGE_FILE_MACHINE_AMD64;
#else // TODO: Write the rest of these, although not likely to support ARM Win
        stackFrame.AddrPC.Offset = context->Eip;
        stackFrame.AddrStack.Offset = context->Esp;
        stackFrame.AddrFrame.Offset = context->Ebp;
        machineType = IMAGE_FILE_MACHINE_I386;
#endif

        uint32_t numEntries = 0;
        while (true)
        {
            if (!StackWalk64(machineType, hProcess, hThread, &stackFrame, context, nullptr, SymFunctionTableAccess64,
                             SymGetModuleBase64, nullptr))
                break;
            if (numEntries < MAX_STACKTRACE_DEPTH)
                stackTrace[numEntries] = stackFrame.AddrPC.Offset;
            numEntries++;
            if (stackFrame.AddrPC.Offset = 0 || stackFrame.AddrFrame.Offset == 0)
                break;
        }
        return std::min(numEntries, MAX_STACKTRACE_DEPTH);
    }

    String GetStackTrace(CONTEXT* context, uint32_t skip = 0)
    {
        uint64_t rawStackTrace[MAX_STACKTRACE_DEPTH];
        uint32_t numEntries = GetRawStackTrace(context, rawStackTrace);

        uint32_t bufferSize = sizeof(PIMAGEHLP_SYMBOL64) + MAX_STACKTRACE_NAME;
        PIMAGEHLP_SYMBOL64 buffer = (PIMAGEHLP_SYMBOL64)std::malloc(bufferSize);
        buffer->SizeOfStruct = bufferSize;
        buffer->MaxNameLength = MAX_STACKTRACE_NAME;
        HANDLE hProcess = GetCurrentProcess();

        StringStream outputStream;
        for (uint32_t i = skip; i < numEntries; i++)
        {
            if (i > skip)
                outputStream << std::endl << "\n";
            DWORD64 funcAddr = rawStackTrace[i];
            DWORD64 displacement;
            if (SymGetSymFromAddr64(hProcess, funcAddr, &displacement, buffer))
            {
                std::vector<char> und_name(MAX_STACKTRACE_NAME);
                UnDecorateSymbolName(buffer->Name, &und_name[0], MAX_STACKTRACE_NAME, UNDNAME_COMPLETE);
                outputStream << fmt::format("{0} - ", buffer->Name);
                outputStream << "\n\t" << &und_name[0];
            }
            IMAGEHLP_LINE64 lineData;
            lineData.SizeOfStruct = sizeof(lineData);
            DWORD col;
            if (SymGetLineFromAddr64(hProcess, funcAddr, &col, &lineData))
            {
                Path filePath = lineData.FileName;
                outputStream << fmt::format("0x{0:x}, File[{1}:{2} ({3})]", funcAddr, filePath, (uint32_t)lineData.LineNumber, (uint32_t)col);
            }
            else
                outputStream << fmt::format("0x{0:x}", funcAddr);

            IMAGEHLP_MODULE64 moduleData;
            moduleData.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            if (SymGetModuleInfo64(hProcess, funcAddr, &moduleData))
            {
                Path filePath = moduleData.ImageName;
                outputStream << fmt::format(" Module[{0}]", filePath);
            }
        }
        free(buffer);
        return outputStream.str();
    }

    void PopupErrorMessageBox(const WString& message, const Path& miniDumpPath)
    {
        WString errorMessage = message + L"\nFor more information check the crash report in: " + miniDumpPath.c_str();
#ifdef CW_EDITOR
        MessageBoxW(nullptr, errorMessage.c_str(), L"Crowny Editor", MB_OK);
#else
        MessageBoxW(nullptr, errorMessage.c_str(), L"Crowny Runtime", MB_OK);
#endif
    }

    Path GetCrashDirectory()
    {
        const Path crashPath = PlatformUtils::GetOurRoamingDirectory() / s_CrashDirectory;
        if (!fs::exists(crashPath))
            fs::create_directory(crashPath);
        return crashPath;
    }

    int CrashHandler::ReportCrash(void* rawExcpetionData)
    {
        EXCEPTION_POINTERS* exceptionData = static_cast<EXCEPTION_POINTERS*>(rawExcpetionData);
        Lock lock(s_Data->Mutex);
        Win32LoadSymbols();
        CW_ENGINE_WARN(Win32GetExceptionMessage(exceptionData->ExceptionRecord));
        CW_ENGINE_WARN(GetStackTrace(exceptionData->ContextRecord));
        WriteMiniDump(GetCrashDirectory() / s_MiniDumpName, exceptionData);
        PopupErrorMessageBox(s_FatalErrorMessage, GetCrashDirectory());

        // DebugBreak();
        return EXCEPTION_EXECUTE_HANDLER;
    }
} // namespace Crowny