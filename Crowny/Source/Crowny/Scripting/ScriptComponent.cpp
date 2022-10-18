#include "cwpch.h"

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{

    ScriptComponentBase::ScriptComponentBase(MonoObject* instance) : ScriptSceneObjectBase(instance) {}
    ScriptComponent::ScriptComponent(MonoObject* instance) : ScriptObject(instance) {}

    void ScriptComponent::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
    }

    MonoObject* ScriptComponent::Internal_GetEntity(ScriptComponentBase* nativeInstance)
    {
        Entity native = nativeInstance->GetNativeEntity();
        ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(native);
        return scriptEntity->GetManagedInstance();
    }

} // namespace Crowny