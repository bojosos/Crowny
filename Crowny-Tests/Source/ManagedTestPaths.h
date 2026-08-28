#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <cstdlib>

namespace Crowny::Test
{
    inline String ReadEnvironmentVariable(const char* name)
    {
#ifdef CW_PLATFORM_WIN32
        char* value = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
            return {};

        String result(value);
        std::free(value);
        return result;
#else
        const char* value = std::getenv(name);
        return value != nullptr ? String(value) : String();
#endif
    }

    inline Path ResolveManagedAssembly(const char* filename, const Path& fallback)
    {
        const String configuredRoot = ReadEnvironmentVariable("CROWNY_MANAGED_ASSEMBLY_ROOT");
        if (!configuredRoot.empty())
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
