#pragma once

#include <Crowny/Common/Module.h>

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{

    struct AssemblyRefreshInfo
    {
        AssemblyRefreshInfo() = default;
        AssemblyRefreshInfo(const char* name, const Path* path /*, const BuiltinTypeMappings* typeMappings*/)
          : Name(name), Filepath(path)
        {
        } //, TypeMappings(typeMappings) { }

        const char* Name = nullptr;
        const Path* Filepath = nullptr;
        // const BuiltinTypeMappings* TypeMappings = nullptr;
    };

    class ScriptObjectManager : public Module<ScriptObjectManager>
    {
    public:
        ScriptObjectManager() = default;
        ~ScriptObjectManager();

        void RegisterScriptObject(ScriptObjectBase* instance);
        void UnregisterScriptObject(ScriptObjectBase* instance);

        void RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies);
        void Update();
        void NotifyObjectFinalized(ScriptObjectBase* instance);
        void ProcessFinalizedObjects(bool assemblyRefresh = false);

    private:
        Set<ScriptObjectBase*> m_ScriptObjects;
        Vector<ScriptObjectBase*> m_FinalizedObjects[2];
        uint32_t m_FinalizedQueueIdx;
    };

} // namespace Crowny