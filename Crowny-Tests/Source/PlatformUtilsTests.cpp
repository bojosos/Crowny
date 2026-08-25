#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/CrashHandler.h"
#include "Crowny/Common/PlatformUtils.h"

#if defined(__linux__)
using namespace Crowny;

TEST_CASE("Linux platform utilities provide process and config services", "[Platform][Linux]")
{
    CHECK(PlatformUtils::Exec("printf crowny-platform-utils") == "crowny-platform-utils");

    const Path roamingDirectory = PlatformUtils::GetRoamingDirectory();
    CHECK_FALSE(roamingDirectory.empty());
    CHECK(roamingDirectory.is_absolute());
}

TEST_CASE("Non-Windows crash handler has a portable fallback", "[Platform][Linux][CrashHandler]")
{
    CrashHandler crashHandler;
    CHECK(crashHandler.ReportCrash(nullptr) == 0);
}
#endif
