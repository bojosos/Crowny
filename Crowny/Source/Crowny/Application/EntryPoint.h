#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"

#include "Crowny/Application/CrashHandler.h"

#if CW_WINDOWS
#include <excpt.h>
#include <processthreadsapi.h>
#endif

extern Crowny::Application* Crowny::CreateApplication();

int main(int argc, char** argv)
{
    Crowny::CrashHandler::StartUp();

#if CW_WINDOWS
    __try
    {
        Crowny::Application* app = Crowny::CreateApplication();
        app->Run();
        delete app;
    }
    __except (Crowny::CrashHandler::Get().ReportCrash(GetExceptionInformation()))
    {
        TerminateProcess(GetCurrentProcess(), 0);
    }
#else
    Crowny::Application* app = Crowny::CreateApplication();
    app->Run();
    delete app;
#endif

    Crowny::CrashHandler::Shutdown();

    return 0;
}
