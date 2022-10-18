#pragma once

#include "Crowny/Scripting/Mono/Mono.h"
#include "Crowny/Scripting/ScriptMeta.h"

#include "Crowny/Common/Module.h"

namespace Crowny
{

    enum class MonoVersion
    {
        v4_5
    };

    class MonoManager : public Module<MonoManager>
    {
    public:
        MonoManager();
        ~MonoManager();

        MonoAssembly& LoadAssembly(const Path& path, const String& name);
        void UnloadAll();
        MonoClass* FindClass(const String& ns, const String& typeName);
        MonoClass* FindClass(::MonoClass* rawClass);
        MonoDomain* GetDomain() const { return m_ScriptDomain; }
        MonoAssembly* GetAssembly(const String& name) const;

        void UnloadDomain();
        Path GetFrameworkAssembliesFolder() const;
        Path GetMonoEtcFolder() const;
        Path GetCompilerPath() const;
        Path GetMonoExecPath() const;

        void UnloadScriptDomain();

        static void RegisterScriptType(ScriptMeta* metaData, const ScriptMeta& localMetaData);

    private:
        struct ScriptMetaInfo
        {
            ScriptMeta* MetaData;
            ScriptMeta LocalMetaData;
        };

        void InitializeScriptTypes(MonoAssembly& assembly);

        // A list of all script types to be initialized per assembly.
        static UnorderedMap<String, Vector<ScriptMetaInfo>>& GetScriptMetaData()
        {
            static UnorderedMap<String, Vector<ScriptMetaInfo>> m_TypesToInitialize;
            return m_TypesToInitialize;
        }

        UnorderedMap<String, MonoAssembly*> m_Assemblies;
        MonoDomain* m_RootDomain;
        MonoDomain* m_ScriptDomain;
        MonoAssembly* m_CorlibAssembly;
    };
} // namespace Crowny