#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Scripting/Mono/Mono.h"

namespace Crowny
{

    // TODO: Add a proper profiler functionality that supports instrumentation.
    class MonoProfiler : public Module<MonoProfiler>
    {
    public:
        struct GarbageCollectionInfo
        {
            uint32_t Allocations = 0;
            uint32_t Deallocations = 0;
            uint32_t MaxGeneration = 0;
            UnorderedSet<MonoObject*> AllocatedObjects;
        };

        MonoProfiler();
        ~MonoProfiler();

        const GarbageCollectionInfo& GetGarbageCollectionInfo() const { return m_GCInfo; }

    private:
        static void OnDomainLoaded(::MonoProfiler* profiler, MonoDomain* domain);
        static void OnDomainUnloaded(::MonoProfiler* profiler, MonoDomain* domain);
        static void OnGCAllocCallback(::MonoProfiler* profiler, MonoObject* object);
        static void OnGCMoveCallback(::MonoProfiler* profiler, MonoObject* const* objects, uint64_t count);
        static void OnGCFinalizingObjectCallback(::MonoProfiler* profiler, MonoObject* object);

        uint32_t m_MaxGeneration = 0;
        bool m_Reloading = false;
        GarbageCollectionInfo m_GCInfo;
        MonoProfilerHandle m_ProfilerHandle;
    };
} // namespace Crowny