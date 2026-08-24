#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"

#include "Crowny/Application/CmdArgs.h"
#include "Crowny/Application/CrashHandler.h"
#include "Crowny/Common/MemoryDiagnostics.h"

#if CW_WINDOWS
#include <excpt.h>
#include <processthreadsapi.h>
#endif

extern void Crowny::CreateApplication();

static void RunCrownyApplication()
{
#if CW_WINDOWS && !defined(CW_ADDRESS_SANITIZER)
    if (!IsDebuggerPresent())
    {
        __try
        {
            Crowny::CreateApplication();
            Crowny::Application::TryGet()->Run();
            Crowny::Application::Shutdown();
        }
        __except (Crowny::CrashHandler::Get().ReportCrash(GetExceptionInformation()))
        {
            TerminateProcess(GetCurrentProcess(), 0);
        }
    }
    else
    {
        Crowny::CreateApplication();
        Crowny::Application::TryGet()->Run();
        Crowny::Application::Shutdown();
    }
#else
    Crowny::CreateApplication();
    Crowny::Application::TryGet()->Run();
    Crowny::Application::Shutdown();
#endif
}

int main(int argc, char** argv)
{
    Crowny::CommandLineArgs::Create(argc, argv);
    Crowny::CrashHandler::StartUp();
    Crowny::ScopedMemoryLeakCheck memoryLeakCheck;

    RunCrownyApplication();

    const bool leakedMemory = memoryLeakCheck.Finish();
    Crowny::CrashHandler::Shutdown();

    return leakedMemory ? 2 : 0;
}
