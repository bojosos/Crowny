#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoProfiler.h"

#include <mono/metadata/profiler.h>

namespace Crowny
{

    MonoProfiler::MonoProfiler()
    {
        m_ProfilerHandle = mono_profiler_create(nullptr);
        mono_profiler_enable_allocations();

        mono_profiler_set_domain_loaded_callback(m_ProfilerHandle, OnDomainLoaded);
        mono_profiler_set_domain_unloaded_callback(m_ProfilerHandle, OnDomainUnloaded);

        mono_profiler_set_gc_allocation_callback(m_ProfilerHandle, OnGCAllocCallback);
        mono_profiler_set_gc_finalized_object_callback(m_ProfilerHandle, OnGCFinalizingObjectCallback);
        // When the GC runs and objects are deleted, some of the remaining ones are copied and
        // the heap shrinks. That's why you can't simply keep MonoObject-s and need gchandles.
        mono_profiler_set_gc_moves_callback(m_ProfilerHandle, OnGCMoveCallback);
    }

    MonoProfiler::~MonoProfiler()
    {
        mono_profiler_set_domain_loaded_callback(m_ProfilerHandle, nullptr);
        mono_profiler_set_domain_unloaded_callback(m_ProfilerHandle, nullptr);

        mono_profiler_set_gc_allocation_callback(m_ProfilerHandle, nullptr);
        mono_profiler_set_gc_finalized_object_callback(m_ProfilerHandle, nullptr);
        mono_profiler_set_gc_moves_callback(m_ProfilerHandle, nullptr);
    }

    void MonoProfiler::OnDomainLoaded(::MonoProfiler* profiler, ::MonoDomain* domain)
    {
        MonoProfiler& profilerInstance = Get();
        profilerInstance.m_Reloading = false;
        profilerInstance.m_MaxGeneration = mono_gc_max_generation();
    }

    void MonoProfiler::OnDomainUnloaded(::MonoProfiler* profiler, MonoDomain* domain)
    {
        MonoProfiler& profilerInstance = Get();
        profilerInstance.m_Reloading = true;
        profilerInstance.m_GCInfo.AllocatedObjects.clear();
        profilerInstance.m_GCInfo.Allocations = 0;
        profilerInstance.m_GCInfo.Deallocations = 0;
    }

    void MonoProfiler::OnGCAllocCallback(::MonoProfiler* profiler, MonoObject* object)
    {
        MonoProfiler& profilerInstance = Get();
        if (profilerInstance.m_Reloading)
            return;

        if (mono_object_get_domain(object) != MonoManager::Get().GetDomain())
            return;
        if (mono_object_get_vtable(object) == nullptr)
            return;
        if (MonoUtils::GetClassName(object).find_first_of("system.") != String::npos)
            return;

        profilerInstance.m_GCInfo.Allocations++;
        profilerInstance.m_GCInfo.AllocatedObjects.insert(object);
    }

    void MonoProfiler::OnGCMoveCallback(::MonoProfiler* profiler, MonoObject* const* objects, uint64_t count)
    {
        MonoProfiler& profilerInstance = Get();
        if (profilerInstance.m_Reloading)
            return;

        for (int i = 0; i < count; i += 2)
        {
            MonoObject* src = objects[i];
            MonoObject* dst = objects[i + 1];
            bool wasTracked = profilerInstance.m_GCInfo.AllocatedObjects.erase(src);
            if (wasTracked)
            {
                profilerInstance.m_GCInfo.AllocatedObjects.insert(dst);
            }
        }
    }

    void MonoProfiler::OnGCFinalizingObjectCallback(::MonoProfiler* profiler, MonoObject* object)
    {
        MonoProfiler& profilerInstance = Get();
        if (profilerInstance.m_Reloading)
            return;
        profilerInstance.m_GCInfo.AllocatedObjects.erase(object);
        profilerInstance.m_GCInfo.Deallocations++;
    }
} // namespace Crowny