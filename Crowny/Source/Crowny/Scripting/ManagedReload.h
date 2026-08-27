#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <chrono>

namespace Crowny
{
    class ManagedReloadDebouncer
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        explicit ManagedReloadDebouncer(std::chrono::milliseconds quietPeriod = std::chrono::milliseconds(1000));

        void Notify(TimePoint now = Clock::now());
        bool TryBegin(TimePoint now = Clock::now());
        void Complete();

        bool IsPending() const { return m_Pending; }
        bool IsBuilding() const { return m_Building; }
        uint64_t GetGeneration() const { return m_Generation; }

    private:
        std::chrono::milliseconds m_QuietPeriod;
        TimePoint m_LastChange{};
        uint64_t m_Generation = 0;
        bool m_Pending = false;
        bool m_Building = false;
    };

    struct MonoRuntimePaths
    {
        Path Root;
        Path LibraryDirectory;
        Path EtcDirectory;
        Path Compiler;

        bool HasRuntime() const;
        bool HasCompiler() const;
    };

    MonoRuntimePaths ResolveMonoRuntimePaths(const Path& workingDirectory);
    MonoRuntimePaths ResolveMonoRuntimePaths(const Vector<Path>& roots);

    bool PublishManagedArtifact(const Path& stagedArtifact, const Path& destinationArtifact, String* error = nullptr);
    bool PublishManagedAssembly(const Path& stagedAssembly, const Path& destinationAssembly, String* error = nullptr);
} // namespace Crowny
