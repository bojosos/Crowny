#include "cwpch.h"

#include "Crowny/Scripting/ScriptObject.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

namespace Crowny
{

    ScriptObjectBase::ScriptObjectBase(MonoObject* instance) { ScriptObjectManager::Get().RegisterScriptObject(this); }

    ScriptObjectBase::~ScriptObjectBase() { ScriptObjectManager::Get().UnregisterScriptObject(this); }

    ScriptObjectBackupData ScriptObjectBase::BeginRefresh() { return ScriptObjectBackupData(); }

    void ScriptObjectBase::EndRefresh(const ScriptObjectBackupData& data) {}

    void ScriptObjectBase::OnManagedInstanceDeleted(bool assemblyRefresh) { delete this; }

    PersistentScriptObjectBase::PersistentScriptObjectBase(MonoObject* instance) : ScriptObjectBase(instance) {}

    void ScriptObjectWrapper::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_ManagedInstanceDeleted",
                                              (void*)&ScriptObjectWrapper::Internal_ManagedInstanceDeleted);
    }

    void ScriptObjectWrapper::Internal_ManagedInstanceDeleted(ScriptObjectBase* instance)
    {
        ScriptObjectManager::Get().NotifyObjectFinalized(instance);
    }

} // namespace Crowny