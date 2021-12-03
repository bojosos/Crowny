#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

#include "Crowny/Scene/SceneManager.h"

namespace Crowny
{

    ScriptEntity::ScriptEntity(MonoObject* instance, Entity entity) : ScriptObject(instance)
    {
        m_Entity = entity;
        SetManagedInstance(instance);
    }

    void ScriptEntity::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetName", (void*)&Internal_GetName);
        MetaData.ScriptClass->AddInternalCall("Internal_SetName", (void*)&Internal_SetName);
        MetaData.ScriptClass->AddInternalCall("Internal_GetParent", (void*)&Internal_GetParent);
        MetaData.ScriptClass->AddInternalCall("Internal_SetParent", (void*)&Internal_SetParent);
        MetaData.ScriptClass->AddInternalCall("Internal_FindByName", (void*)&Internal_FindEntityByName);

        MetaData.ScriptClass->AddInternalCall("Internal_GetComponent", (void*)&Internal_GetComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_HasComponent", (void*)&Internal_HasComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_AddComponent", (void*)&Internal_AddComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_RemoveComponent", (void*)&Internal_RemoveComponent);
    }

    MonoString* ScriptEntity::Internal_GetName(ScriptEntity* thisPtr)
    {
        const String& cStr = thisPtr->GetNativeEntity().GetName();
        return MonoUtils::ToMonoString(cStr);
    }

    void ScriptEntity::Internal_SetName(ScriptEntity* thisPtr, MonoString* string)
    {
        thisPtr->GetNativeEntity().GetComponent<TagComponent>().Tag = MonoUtils::FromMonoString(string);
    }

    MonoObject* ScriptEntity::Internal_GetParent(ScriptEntity* thisPtr)
    {
        ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(thisPtr->GetNativeEntity().GetParent());
        if (scriptEntity != nullptr)
            return scriptEntity->GetManagedInstance();
        return nullptr;
    }

    void ScriptEntity::Internal_SetParent(ScriptEntity* thisPtr, MonoObject* parent)
    {
        ScriptEntity* scriptParent = ScriptEntity::ToNative(parent);
        if (thisPtr != nullptr && scriptParent != nullptr)
            thisPtr->GetNativeEntity().SetParent(scriptParent->GetNativeEntity());
    }

    MonoObject* ScriptEntity::Internal_FindEntityByName(MonoString* name)
    {
        Entity e = SceneManager::GetActiveScene()->FindEntityByName(MonoUtils::FromMonoString(name));
        if (e)
            return ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(e)->GetManagedInstance();
        return nullptr;
    }

    MonoObject* ScriptEntity::Internal_GetComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        Entity entity = thisPtr->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (!info->HasCallback(entity))
            return nullptr;
        return info->GetCallback(entity)->GetManagedInstance();
    }

    bool ScriptEntity::Internal_HasComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        Entity entity = thisPtr->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return false;
        return info->HasCallback(entity);
    }

    MonoObject* ScriptEntity::Internal_AddComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        Entity entity = thisPtr->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (info->HasCallback(entity))
            return nullptr;
        return info->AddCallback(entity)->GetManagedInstance();
    }

    void ScriptEntity::Internal_RemoveComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        Entity entity = thisPtr->GetNativeEntity();
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
        {
            if (info->HasCallback(entity))
                info->RemoveCallback(entity);
            else
                CW_ENGINE_ERROR("Entity doesn't have that component");
        }
        else
            CW_ENGINE_ERROR("That is not a component");
    }

} // namespace Crowny