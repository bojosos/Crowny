#pragma once

#include <Crowny/Common/Module.h>

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{

    struct AssemblyRefreshInfo
    {
        AssemblyRefreshInfo() = default;
        AssemblyRefreshInfo(const char* name, const Path* path /*, const BuiltinTypeMappings* typeMappings*/)
          : Name(name != nullptr ? name : ""), Filepath(path != nullptr ? *path : Path())
        {
        } //, TypeMappings(typeMappings) { }
        AssemblyRefreshInfo(String name, Path path) : Name(std::move(name)), Filepath(std::move(path)) {}

        String Name;
        Path Filepath;
        // const BuiltinTypeMappings* TypeMappings = nullptr;
    };

    class ScriptObjectManager : public Module<ScriptObjectManager>
    {
    public:
        ScriptObjectManager() = default;
        ~ScriptObjectManager();

        void RegisterScriptObject(ScriptObjectBase* instance);
        void UnregisterScriptObject(ScriptObjectBase* instance);

        bool RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies);
        void Update();
        void NotifyObjectFinalized(ScriptObjectBase* instance);
        void ProcessFinalizedObjects(bool assemblyRefresh = false);

    private:
        Set<ScriptObjectBase*> m_ScriptObjects;
        Vector<ScriptObjectBase*> m_FinalizedObjects[2];
        uint32_t m_FinalizedQueueIdx = 0;
        Mutex m_Mutex;
    };

} // namespace Crowny
