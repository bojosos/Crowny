#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <cstdlib>

namespace Crowny::Test
{
    inline Path ResolveManagedAssembly(const char* filename, const Path& fallback)
    {
        if (const char* configuredRoot = std::getenv("CROWNY_MANAGED_ASSEMBLY_ROOT"); configuredRoot != nullptr && configuredRoot[0] != '\0')
        {
            const Path configured = Path(configuredRoot) / filename;
            if (fs::is_regular_file(configured))
                return fs::absolute(configured);
        }

#ifdef CW_DEBUG
        constexpr const char* configuration = "Debug";
#elif defined(CW_DIST)
        constexpr const char* configuration = "Dist";
#else
        constexpr const char* configuration = "Release";
#endif
        Path candidate = fs::current_path();
        for (uint32_t depth = 0; depth < 8 && !candidate.empty(); depth++)
        {
            const Path generated = candidate / ".deps/generated/managed" / configuration / filename;
            if (fs::is_regular_file(generated))
                return fs::absolute(generated);
            const Path sourceFallback = candidate / fallback;
            if (fs::is_regular_file(sourceFallback))
                return fs::absolute(sourceFallback);
            candidate = candidate.parent_path();
        }
        return fs::absolute(fallback);
    }
} // namespace Crowny::Test
