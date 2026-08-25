#include <catch2/catch_session.hpp>

#include "Crowny/Common/Log.h"
#include "Crowny/Common/MemoryDiagnostics.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <werapi.h>
#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    WerSetFlags(WER_FAULT_REPORTING_NO_UI);
#endif

    Crowny::Log::Init("CrownyTests");
    Crowny::ScopedMemoryLeakCheck memoryLeakCheck;
    int result = 0;
    {
        Catch::Session session;
        result = session.run(argc, argv);
    }
    if (memoryLeakCheck.Finish() && result == 0)
        result = 2;
    return result;
}
