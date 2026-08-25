#include "cwpch.h"

#include "Crowny/Application/CrashHandler.h"

#ifndef CW_PLATFORM_WIN32
namespace Crowny
{
    CrashHandler::CrashHandler() = default;

    CrashHandler::~CrashHandler() = default;

    int CrashHandler::ReportCrash(void*) { return 0; }
} // namespace Crowny
#endif
