#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

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
        MetaData.ScriptClass->AddInternalCall("Internal_GetUUID", (void*)&Internal_GetUUID);
        MetaData.ScriptClass->AddInternalCall("Internal_FindByName", (void*)&Internal_FindEntityByName);

        MetaData.ScriptClass->AddInternalCall("Internal_GetComponent", (void*)&Internal_GetComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_HasComponent", (void*)&Internal_HasComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_AddComponent", (void*)&Internal_AddComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_RemoveComponent", (void*)&Internal_RemoveComponent);
        MetaData.ScriptClass->AddInternalCall("Internal_Destroy", (void*)&Internal_Destroy);
    }

    void ScriptEntity::Internal_Destroy(ScriptEntity* thisPtr)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        SceneManager::TryGet()->GetActiveScene()->DestroyEntity(entity);
    }

    MonoString* ScriptEntity::Internal_GetName(ScriptEntity* thisPtr)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        const String& cStr = entity.GetName();
        return MonoUtils::ToMonoString(cStr);
    }

    void ScriptEntity::Internal_SetName(ScriptEntity* thisPtr, MonoString* string)
    {
        Entity entity = thisPtr->GetNativeEntity();
        TagComponent& tagComponent = entity.GetComponent<TagComponent>();
        tagComponent.Tag = MonoUtils::FromMonoString(string);
    }

    void ScriptEntity::Internal_GetUUID(ScriptEntity* thisPtr, UUID* uuid)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        *uuid = entity.GetUuid();
    }

    MonoObject* ScriptEntity::Internal_GetParent(ScriptEntity* thisPtr)
    {
        const Entity parent = thisPtr->GetNativeEntity().GetParent();
        ScriptEntity* const scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(parent);
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
        const Entity entity = SceneManager::TryGet()->GetActiveScene()->FindEntityByName(MonoUtils::FromMonoString(name));
        ScriptEntity* const scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity);
        if (scriptEntity != nullptr)
            return scriptEntity->GetManagedInstance();
        return nullptr;
    }

    MonoObject* ScriptEntity::Internal_GetComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* const componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(
              componentClass,
              ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetInternalPtr())) // We are trying to retrieve a behavior, so
                                                                                               // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                return nullptr;
            const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
            for (const auto& script : scripts)
            {
                MonoClass* managedClass = script.GetManagedClass();
                if (managedClass != nullptr && MonoUtils::IsSubClassOf(managedClass->GetInternalPtr(), componentClass))
                    return script.GetManagedInstance();
            }
            return nullptr;
        }

        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (!info->HasCallback(entity))
            return nullptr;
        return info->GetCallback(entity)->GetManagedInstance();
    }

    bool ScriptEntity::Internal_HasComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* const componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(
              componentClass,
              ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetInternalPtr())) // We are trying to check for a behavior, so
                                                                                               // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                return false;
            else
            {
                const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
                for (const auto& script : scripts)
                {
                    MonoClass* managedClass = script.GetManagedClass();
                    if (managedClass != nullptr && MonoUtils::IsSubClassOf(managedClass->GetInternalPtr(), componentClass))
                        return true;
                }
                return false;
            }
        }

        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return false;
        return info->HasCallback(entity);
    }

    MonoObject* ScriptEntity::Internal_AddComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        const Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* const componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(
              componentClass,
              ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetInternalPtr())) // We are trying to add a behavior, so loop
                                                                                               // the MonoScriptBehaviour.Scripts
        {
            MonoAssembly* assembly = MonoManager::Get().FindAssembly(componentClass);
            if (assembly == nullptr)
            {
                CW_ERROR("Could not identify the assembly for managed component");
                return nullptr;
            }
            String namespaceName;
            String typeName;
            MonoUtils::GetClassName(componentClass, namespaceName, typeName);
            const ScriptTypeIdentity identity{ assembly->GetName(), namespaceName, typeName };
            if (!entity.GetScene()->AddScriptComponent(entity, identity))
                return nullptr;
            MonoScriptComponent& scripts = entity.GetComponent<MonoScriptComponent>();
            for (MonoScript& script : scripts.Scripts)
            {
                if (script.GetTypeIdentity() == identity)
                    return script.GetManagedInstance();
            }
            return nullptr;
        }
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info == nullptr)
            return nullptr;
        if (info->HasCallback(entity))
            return nullptr;
        return info->AddCallback(entity)->GetManagedInstance();
    }

    void ScriptEntity::Internal_RemoveComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        const Entity entity = thisPtr->GetNativeEntity();

        ::MonoClass* const componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(
              componentClass,
              ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetInternalPtr())) // We are trying to remove a behavior, so
                                                                                               // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                CW_ERROR("Entity doesn't have that component");
            else
            {
                MonoAssembly* assembly = MonoManager::Get().FindAssembly(componentClass);
                String namespaceName;
                String typeName;
                MonoUtils::GetClassName(componentClass, namespaceName, typeName);
                const ScriptTypeIdentity identity{ assembly != nullptr ? assembly->GetName() : String(), namespaceName, typeName };
                if (assembly == nullptr || !entity.GetScene()->HasScriptComponent(entity, identity))
                    CW_ERROR("Entity doesn't have that component");
                else
                    entity.GetScene()->RemoveScriptComponent(entity, identity);
            }
            return;
        }

        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(type);
        if (info != nullptr)
        {
            if (info->HasCallback(entity))
                info->RemoveCallback(entity);
            else
                CW_ERROR("Entity doesn't have that component");
        }
        else
            CW_ERROR("That is not a component");
    }

} // namespace Crowny
