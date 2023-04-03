#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"

#include "Crowny/Common/FileSystem.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/profiler.h>
#include <mono/metadata/threads.h>
#include <mono/utils/mono-logger.h>

namespace Crowny
{

    const String MONO_LIB_DIR = "bin/Mono/lib";
    const String MONO_ETC_DIR = "bin/Mono/etc";
    const String MONO_COMPILER_DIR = "bin/Mono/compiler";
    const MonoVersion MONO_VERSION = MonoVersion::v4_5;

    struct MonoVersionData
    {
        String Path;
        String Version;
    };

    static const MonoVersionData MONO_VERSION_DATA[1] = { { MONO_LIB_DIR + "mono/4.8", "v4.0.30319" } };

    void MonoLogCallback(const char* logDomain, const char* logLevel, const char* message, mono_bool fatal,
                         void* userData)
    {
        static const char* monoErrorLevels[] = { nullptr, "error", "critical", "warning", "message", "info", "debug" };

        uint32_t errorLevel = 0;
        if (logLevel != nullptr)
        {
            for (uint32_t i = 1; i < 7; i++)
            {
                if (std::strcmp(monoErrorLevels[i], logLevel) == 0)
                {
                    errorLevel = i;
                    break;
                }
            }
        }
        // CW_ENGINE_INFO("Here");
        if (logDomain == nullptr)
            logDomain = "Null";
        if (errorLevel == 0)
            CW_ENGINE_ERROR("Mono: {0} in {1}", message, logDomain);
        else if (errorLevel <= 2)
            CW_ENGINE_ERROR("Mono: {0} in {1}", message, logDomain);
        else if (errorLevel <= 3)
            CW_ENGINE_WARN("Mono: {0} in {1}", message, logDomain);
        else
            CW_ENGINE_INFO("Mono: {0} in {1}", message, logDomain);
    }

    void MonoPrintCallback(const char* string, mono_bool isStdout) { CW_ENGINE_ERROR("Mono: {0}", string); }

    void MonoPrintErrorCallback(const char* string, mono_bool isStdout) { CW_ENGINE_ERROR("Mono: {0}", string); }
    /*

     struct _MonoProfiler
    {
            const char* type_name;
            Crowny::Vector<Crowny::String> gchandles;
            Crowny::Vector<Crowny::String> stacktraces;
            Crowny::Mutex mutex; // used to ensure only one thread accesses the arrays
    };


    static void MethodEnterCallback(MonoProfiler* profiler, ::MonoMethod* method, MonoProfilerCallContext* context)
    {
        // CW_ENGINE_INFO("Method {0} enter", MonoMethod(method).GetName());
    }

    static void MethodLeaveCallback(MonoProfiler* profiler, ::MonoMethod* method, MonoProfilerCallContext* context)
    {
        // CW_ENGINE_INFO("Method {0} leave", MonoMethod(method).GetName());
    }

    static void GCHandleCreated(MonoProfiler* profiler, uint32_t handle, MonoGCHandleType type, MonoObject* object)
    {
        if (type == MONO_GC_HANDLE_NORMAL)
        {
            // CW_ENGINE_INFO("Created handle of type: {0}", MonoUtils::GetClassName(object));
        }
    }

    static MonoProfilerCallInstrumentationFlags InstrumentationFilter(MonoProfiler* prof, ::MonoMethod* method)
    {
        return (MonoProfilerCallInstrumentationFlags)(
          MONO_PROFILER_CALL_INSTRUMENTATION_ENTER | MONO_PROFILER_CALL_INSTRUMENTATION_LEAVE |
          MONO_PROFILER_CALL_INSTRUMENTATION_TAIL_CALL | MONO_PROFILER_CALL_INSTRUMENTATION_EXCEPTION_LEAVE);
    }

    static void GCHandleDestroyed(MonoProfiler* profiler, uint32_t handle, MonoGCHandleType type)
    {
        if (type == MONO_GC_HANDLE_NORMAL)
        {
            CW_ENGINE_INFO("Deleted handle of type: {0}",
                           MonoUtils::GetClassName(MonoUtils::GetObjectFromGCHandle(handle)));
        }
    }

    static MonoProfiler profiler;
    */
    MonoManager::MonoManager() : m_ScriptDomain(nullptr), m_RootDomain(nullptr), m_CorlibAssembly(nullptr)
    {
        /*
        if (Application::Get().GetApplicationDesc().Script.EnableProfiling)
        {
            MonoProfilerHandle profilerHandle = mono_profiler_create(&profiler);
            mono_profiler_set_method_enter_callback(profilerHandle, MethodEnterCallback);
            mono_profiler_set_method_leave_callback(profilerHandle, MethodLeaveCallback);
            mono_profiler_set_call_instrumentation_filter_callback(profilerHandle, InstrumentationFilter);
            mono_profiler_set_gc_handle_created_callback(profilerHandle, GCHandleCreated);
            mono_profiler_set_gc_handle_deleted_callback(profilerHandle, GCHandleDestroyed);
        }
        */
        Path libDir = MONO_LIB_DIR;
        Path etcDir = GetMonoEtcFolder();
        Path assembliesDir = GetFrameworkAssembliesFolder();

        // mono_set_dirs(libDir.c_str(), etcDir.c_str());
        mono_set_assemblies_path("C:\\Program Files\\Mono\\lib");
        // TODO: Fix these dirs
        mono_set_dirs("C:\\Program Files\\Mono\\lib", "C:\\Program Files\\Mono\\etc");

        // #if CW_DEBUG
        mono_debug_init(MONO_DEBUG_FORMAT_MONO);
        mono_trace_set_level_string("debug");
        mono_trace_set_print_handler(MonoPrintCallback);
        // TODO: Add port settings
        const char* options[] = {
            "--soft-breakpoints",
            "--debugger-agent=transport=dt_socket,address=127.0.0.1:17615,embedding=1,server=y,suspend=n",
            "--debug-domain-unload", "--gc-debug=check-remset-consistency,verify-before-collections,xdomain-checks"
        };
        mono_jit_parse_options(4, (char**)options);
        // mono_trace_set_level_string("warning"); // maybe do debug
        // #else
        mono_trace_set_level_string("debug");
        // #endif
        mono_trace_set_log_handler(MonoLogCallback, this);
        mono_trace_set_print_handler(MonoPrintCallback);
        mono_trace_set_printerr_handler(MonoPrintErrorCallback);
        mono_config_parse(nullptr);
        // m_RootDomain = mono_jit_init_version("CrownyMono", MONO_VERSION_DATA[(int)MONO_VERSION].Version.c_str());
        m_RootDomain = mono_jit_init("CrownyMono");
        if (m_RootDomain == nullptr)
        {
            CW_ENGINE_ERROR("Cannot initialize mono runtime");
            return;
        }
        mono_debug_domain_create(m_RootDomain);
        mono_thread_set_main(mono_thread_current());
        m_CorlibAssembly = new MonoAssembly("", "corlib");
        m_CorlibAssembly->LoadFromImage(mono_get_corlib());
        m_Assemblies["corlib"] = m_CorlibAssembly;
    }

    MonoManager::~MonoManager() { UnloadAll(); }

    MonoAssembly& MonoManager::LoadAssembly(const Path& path, const String& name)
    {
        MonoAssembly* assembly = nullptr;
        if (m_ScriptDomain == nullptr)
        {
            String appDomainName = "ScriptDomain";
            m_ScriptDomain = mono_domain_create_appdomain(const_cast<char*>(appDomainName.c_str()), nullptr);
            if (m_ScriptDomain == nullptr)
                CW_ENGINE_ERROR("Cannot create script domain");
            if (!mono_domain_set(m_ScriptDomain, true))
                CW_ENGINE_ERROR("Cannot set script domain");
        }

        auto findIter = m_Assemblies.find(name);
        if (findIter != m_Assemblies.end())
            assembly = findIter->second;
        {
            assembly = new MonoAssembly(path, name);
            m_Assemblies[name] = assembly;
        }

        if (!assembly->m_IsLoaded)
        {
            assembly->Load();
            InitializeScriptTypes(*assembly);
        }

        return *assembly;
    }

    void MonoManager::InitializeScriptTypes(MonoAssembly& assembly)
    {
        Vector<ScriptMetaInfo>& typeMetas = GetScriptMetaData()[assembly.m_Name];
        for (auto& entry : typeMetas)
        {
            ScriptMeta* meta = entry.MetaData;
            *meta = entry.LocalMetaData;
            meta->ScriptClass = assembly.GetClass(meta->Namespace, meta->Name);
            CW_ENGINE_ASSERT(meta->ScriptClass != nullptr);
            if (meta->ScriptClass->HasField("m_InternalPtr"))
                meta->CachedPtrField = meta->ScriptClass->GetField("m_InternalPtr");
            else
                meta->CachedPtrField = nullptr;
            meta->InitCallback();
        }
    }

    void MonoManager::UnloadAll()
    {
        for (auto& entry : m_Assemblies)
            delete entry.second;

        m_Assemblies.clear();
        UnloadScriptDomain();
        if (m_RootDomain != nullptr)
        {
            mono_jit_cleanup(m_RootDomain);
            m_RootDomain = nullptr;
        }

        GetScriptMetaData().clear();
    }

    MonoAssembly* MonoManager::GetAssembly(const String& name) const
    {
        auto findIter = m_Assemblies.find(name);
        if (findIter != m_Assemblies.end())
            return findIter->second;

        return nullptr;
    }

    MonoClass* MonoManager::FindClass(const String& ns, const String& typeName)
    {
        MonoClass* monoClass = nullptr;
        for (auto& assembly : m_Assemblies)
        {
            monoClass = assembly.second->GetClass(ns, typeName);
            if (monoClass != nullptr)
                return monoClass;
        }
        return nullptr;
    }

    MonoClass* MonoManager::FindClass(::MonoClass* rawMonoClass)
    {
        MonoClass* monoClass = nullptr;
        for (auto& assembly : m_Assemblies)
        {
            monoClass = assembly.second->GetClass(rawMonoClass);
            if (monoClass != nullptr)
                return monoClass;
        }
        return nullptr;
    }

    void MonoManager::RegisterScriptType(ScriptMeta* metaData, const ScriptMeta& localMetaData)
    {
        GetScriptMetaData()[localMetaData.Assembly].push_back({ metaData, localMetaData });
    }

    void MonoManager::UnloadScriptDomain()
    {
        if (m_ScriptDomain != nullptr)
        {
            mono_domain_set(mono_get_root_domain(), true);
            MonoObject* exception = nullptr;
            mono_domain_try_unload(m_ScriptDomain, &exception);
            if (exception != nullptr)
                MonoUtils::CheckException(exception);
            m_ScriptDomain = nullptr;
        }

        for (auto& assemblyEntry : m_Assemblies)
        {
            assemblyEntry.second->Unload();
            if (assemblyEntry.first != "corlib")
                delete assemblyEntry.second;
            Vector<ScriptMetaInfo>& typeMetas = GetScriptMetaData()[assemblyEntry.first];
            for (auto& entry : typeMetas)
            {
                entry.MetaData->ScriptClass = nullptr;
                entry.MetaData->CachedPtrField = nullptr;
            }
        }

        m_Assemblies.clear();
        m_Assemblies["corlib"] = m_CorlibAssembly;
    }

    Path MonoManager::GetFrameworkAssembliesFolder() const { return MONO_VERSION_DATA[(int)MONO_VERSION].Path; }

    Path MonoManager::GetMonoEtcFolder() const { return MONO_ETC_DIR; }

    Path MonoManager::GetCompilerPath() const { return Path(MONO_COMPILER_DIR) / "mcs"; }

} // namespace Crowny