#pragma once

#include "Crowny/Scripting/CWMonoAssembly.h"

BEGIN_MONO_INCLUDE
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

#define CROWNY_ASSEMBLY "Crowny.dll"
#define CLIENT_ASSEMBLY "Client.dll"
#define CORLIB_ASSEMBLY "corlib"

#define ASSEMBLY_COUNT 3

#define CROWNY_ASSEMBLY_INDEX 0
#define CLIENT_ASSEMBLY_INDEX 1
#define CORLIB_ASSEMBLY_INDEX 2

namespace Crowny
{
    struct BuiltinScriptClasses
    {
        CWMonoClass* SystemArrayClass = nullptr;
        CWMonoClass* SystemGenericListClass = nullptr;
        CWMonoClass* SystemGenericDictionaryClass = nullptr;
        CWMonoClass* SystemTypeClass = nullptr;

        CWMonoClass* SerializeFieldAttribute = nullptr;
        CWMonoClass* RangeAttribute = nullptr;
        CWMonoClass* ShowInInspector = nullptr;
        CWMonoClass* HideInInspector = nullptr;
        CWMonoClass* ScriptUtils = nullptr;
    };

    class CWMonoRuntime
    {
    public:
        static bool Init(const std::string& domainName);
        static void Shutdown();

        static void LoadAssemblies(const std::string& directory);

        static CWMonoAssembly* GetCrownyAssembly() { return s_Instance->m_Assemblies[CROWNY_ASSEMBLY_INDEX]; }
        static CWMonoAssembly* GetClientAssembly() { return s_Instance->m_Assemblies[CLIENT_ASSEMBLY_INDEX]; }
        static CWMonoAssembly* GetCorlibAssembly() { return s_Instance->m_Assemblies[CORLIB_ASSEMBLY_INDEX]; }

        static MonoDomain* GetDomain() { return s_Instance->m_Domain; }
        static BuiltinScriptClasses GetBuiltinClasses() { return s_Instance->m_BuiltinScriptClasses; }

    private:
        static CWMonoRuntime* s_Instance;
        BuiltinScriptClasses m_BuiltinScriptClasses;
        std::vector<CWMonoAssembly*> m_Assemblies;
        MonoDomain* m_Domain;
    };

} // namespace Crowny