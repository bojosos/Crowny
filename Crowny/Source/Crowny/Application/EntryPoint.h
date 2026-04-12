#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"

#include "Crowny/Application/CrashHandler.h"

#if CW_WINDOWS
#include <excpt.h>
#include <processthreadsapi.h>
#endif

extern void Crowny::CreateApplication();

int main(int argc, char** argv)
{
    Crowny::CrashHandler::StartUp();

#if CW_WINDOWS
    if (!IsDebuggerPresent())
    {
        __try
        {
            Crowny::CreateApplication();
            Crowny::gApplication->Run();
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
        Crowny::gApplication->Run();
        Crowny::Application::Shutdown();
    }
#else
    Crowny::CreateApplication();
    Crowny::gApplication->Run();
    Crowny::Application::Shutdown();
#endif

    Crowny::CrashHandler::Shutdown();

    return 0;
}
