#include "cwpch.h"

#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{

    ScriptComponentBase::ScriptComponentBase(MonoObject* instance) : ScriptSceneObjectBase(instance) {}
    ScriptComponent::ScriptComponent(MonoObject* instance) : ScriptObject(instance) {}

    void ScriptComponent::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetComponent", (void*)&Internal_GetComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_HasComponent", (void*)&Internal_HasComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_AddComponent", (void*)&Internal_AddComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_RemoveComponent", (void*)&Internal_RemoveComponent);
    }

    MonoObject* ScriptComponent::Internal_GetEntity(ScriptComponentBase* nativeInstance)
    {
        Entity native = nativeInstance->GetNativeEntity();
        ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(native);
        return scriptEntity->GetManagedInstance();
    }

    MonoObject* ScriptComponent::Internal_GetComponent(MonoObject* parentEntity, MonoReflectionType* type)
    {
        ScriptEntity* scriptEntity = ScriptEntity::ToNative(parentEntity);
        Entity entity = scriptEntity->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (!info->HasCallback(entity))
            return nullptr;
        return info->GetCallback(entity)->GetManagedInstance();
    }

    bool ScriptComponent::Internal_HasComponent(MonoObject* parentEntity, MonoReflectionType* type)
    {
        ScriptEntity* scriptEntity = ScriptEntity::ToNative(parentEntity);
        Entity entity = scriptEntity->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return false;
        return info->HasCallback(entity);
    }

    MonoObject* ScriptComponent::Internal_AddComponent(MonoObject* parentEntity, MonoReflectionType* type)
    {
        ScriptEntity* scriptEntity = ScriptEntity::ToNative(parentEntity);
        Entity entity = scriptEntity->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (!info->HasCallback(entity))
            return nullptr;
        return info->AddCallback(entity)->GetManagedInstance();
    }

    void ScriptComponent::Internal_RemoveComponent(MonoObject* parentEntity, MonoReflectionType* type) {}

} // namespace Crowny