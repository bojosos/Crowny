#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
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
        Entity parent = thisPtr->GetNativeEntity().GetParent();
        ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(parent);
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
        Entity entity = SceneManager::GetActiveScene()->FindEntityByName(MonoUtils::FromMonoString(name));
        ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity);
        if (scriptEntity != nullptr)
            return scriptEntity->GetManagedInstance();
        return nullptr;
    }

    MonoObject* ScriptEntity::Internal_GetComponent(ScriptEntity* thisPtr, MonoReflectionType* type)
    {
        Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(componentClass,
                                    ScriptInfoManager::Get()
                                      .GetBuiltinClasses()
                                      .EntityBehaviour->GetInternalPtr())) // We are trying to retrieve a behavior, so
                                                                           // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                return nullptr;
            const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
            for (auto& script : scripts)
            {
                if (MonoUtils::IsSubClassOf(script.GetManagedClass()->GetInternalPtr(), componentClass))
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
        Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(componentClass,
                                    ScriptInfoManager::Get()
                                      .GetBuiltinClasses()
                                      .EntityBehaviour->GetInternalPtr())) // We are trying to check for a behavior, so
                                                                           // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                return false;
            else
            {
                String ns, ts;
                MonoUtils::GetClassName(componentClass, ns, ts);
                auto& msc = entity.AddComponent<MonoScriptComponent>(ts);
                const auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
                for (auto& script : scripts)
                {
                    if (MonoUtils::IsSubClassOf(script.GetManagedClass()->GetInternalPtr(), componentClass))
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
        Entity entity = thisPtr->GetNativeEntity();
        ::MonoClass* componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(componentClass,
                                    ScriptInfoManager::Get()
                                      .GetBuiltinClasses()
                                      .EntityBehaviour->GetInternalPtr())) // We are trying to add a behavior, so loop
                                                                           // the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
            {
                String ns, ts;
                MonoUtils::GetClassName(componentClass, ns, ts);
                auto& msc = entity.AddComponent<MonoScriptComponent>(ts);
                // FIXME msc.Scripts.back().OnInitialize(entity);
                return msc.Scripts.back().GetManagedInstance();
            }
            else
            {
                String ns, ts;
                MonoUtils::GetClassName(componentClass, ns, ts);
                auto& msc = entity.GetComponent<MonoScriptComponent>();
                const auto& scripts = msc.Scripts;
                for (auto& script : scripts)
                {
                    if (script.GetManagedClass()->GetInternalPtr() == componentClass)
                    {
                        CW_ERROR("Entity already has that component!");
                        return nullptr;
                    }
                }
                msc.Scripts.push_back(MonoScript(ts));
                // FIXME msc.Scripts.back().OnInitialize(entity);
                return msc.Scripts.back().GetManagedInstance();
            }
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
        Entity entity = thisPtr->GetNativeEntity();

        ::MonoClass* componentClass = MonoUtils::GetClass(type);
        if (MonoUtils::IsSubClassOf(componentClass,
                                    ScriptInfoManager::Get()
                                      .GetBuiltinClasses()
                                      .EntityBehaviour->GetInternalPtr())) // We are trying to remove a behavior, so
                                                                           // loop the MonoScriptBehaviour.Scripts
        {
            if (!entity.HasComponent<MonoScriptComponent>())
                CW_ERROR("Entity doesn't have that component");
            else
            {
                String ns, ts;
                MonoUtils::GetClassName(componentClass, ns, ts);
                MonoScriptComponent& scriptComponent = entity.GetComponent<MonoScriptComponent>();
                auto findIter = std::find_if(scriptComponent.Scripts.begin(), scriptComponent.Scripts.end(),
                                             [&](const MonoScript& script) {
                                                 return script.GetManagedClass()->GetInternalPtr() == componentClass;
                                             });
                if (findIter == scriptComponent.Scripts.end())
                    CW_ERROR("Entity doesn't have that component");
                else
                    scriptComponent.Scripts.erase(findIter);
            }
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