#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{

    class CrashHandler : public Module<CrashHandler>
    {
    public:
        CrashHandler();
        ~CrashHandler();

        int ReportCrash(void* data);


    };
} // namespace Crowny