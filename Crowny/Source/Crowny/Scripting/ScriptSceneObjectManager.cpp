#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{
    ScriptSceneObjectManager::EntityKey ScriptSceneObjectManager::GetEntityKey(Entity entity)
    {
        return { entity.GetScene(), entity.GetHandle() };
    }

    ScriptEntity* ScriptSceneObjectManager::GetOrCreateScriptEntity(Entity entity)
    {
        if (!entity)
            return nullptr;
        ScriptEntity* scriptEntity = GetScriptEntity(entity);
        if (scriptEntity != nullptr)
            return scriptEntity;
        return CreateScriptEntity(entity);
    }

    ScriptEntity* ScriptSceneObjectManager::CreateScriptEntity(Entity entity)
    {
        MonoClass* entityClass = ScriptInfoManager::Get().GetBuiltinClasses().Entity;
        MonoObject* instance = entityClass->CreateInstance();
        return CreateScriptEntity(instance, entity);
    }

    ScriptEntity* ScriptSceneObjectManager::CreateScriptEntity(MonoObject* existingInstance, Entity entity)
    {
        ScriptEntity* scriptEntity = GetScriptEntity(entity);
        if (scriptEntity != nullptr)
        {
            CW_ENGINE_ERROR("This object already exists");
            return scriptEntity;
        }

        ScriptEntity* nativeInstance = new ScriptEntity(existingInstance, entity);
        m_ScriptEntities[GetEntityKey(entity)] = nativeInstance;
        return nativeInstance;
    }

    ScriptEntity* ScriptSceneObjectManager::GetScriptEntity(Entity entity) const
    {
        auto findIter = m_ScriptEntities.find(GetEntityKey(entity));
        if (findIter != m_ScriptEntities.end())
            return findIter->second;
        return nullptr;
    }

    void ScriptSceneObjectManager::DestroyScriptEntity(ScriptEntity* scriptEntity)
    {
        m_ScriptEntities.erase(GetEntityKey(scriptEntity->GetNativeEntity()));
        scriptEntity->NotifyDestroyed();
        delete scriptEntity;
    }

    ScriptComponentBase* ScriptSceneObjectManager::GetScriptComponent(Entity entity, const ComponentBase& component, MonoReflectionType* reflType,
                                                                      bool create)
    {
        ScriptComponentBase* scriptComponent = GetScriptComponent(component.InstanceId);
        if (scriptComponent != nullptr)
            return scriptComponent;
        if (create)
            return CreateScriptComponent(entity, component, reflType);
        return nullptr;
    }

    ScriptComponentBase* ScriptSceneObjectManager::GetScriptComponent(uint64_t instanceId)
    {
        auto iterFind = m_ScriptComponents.find(instanceId);
        if (iterFind != m_ScriptComponents.end())
            return iterFind->second;
        return nullptr;
    }

    ScriptComponentBase* ScriptSceneObjectManager::CreateScriptComponent(Entity entity, const ComponentBase& component, MonoReflectionType* reflType)
    {
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(reflType);
        if (info == nullptr)
            return nullptr;
        ScriptComponentBase* nativeInstance = info->CreateCallback(entity);
        nativeInstance->SetNativeEntity(entity);
        const uint64_t instanceId = component.InstanceId;
        m_ScriptComponents[instanceId] = nativeInstance;

        return nativeInstance;
    }

    ScriptEntityBehaviour* ScriptSceneObjectManager::CreateManagedScriptComponent(MonoObject* instance, Entity entity, MonoScript& script)
    {
        ScriptEntityBehaviour* nativeInstance = new ScriptEntityBehaviour(instance, entity, script);
        m_ScriptComponents[script.InstanceId] = nativeInstance;
        return nativeInstance;
    }

    void ScriptSceneObjectManager::DestroyScriptComponent(ScriptComponentBase* scriptComponent, uint64_t instanceId)
    {
        m_ScriptComponents.erase(instanceId);
        scriptComponent->NotifyDestroyed();
        delete scriptComponent;
    }

    void ScriptSceneObjectManager::DestroyManagedScriptComponent(Entity entity, MonoScript* script)
    {
        if (!entity || script == nullptr)
            return;
        const auto iter = m_ScriptComponents.find(script->InstanceId);
        if (iter == m_ScriptComponents.end() || iter->second->GetNativeEntity() != entity)
            return;
        DestroyScriptComponent(iter->second, script->InstanceId);
        script->ClearRuntimeInstance();
    }

    void ScriptSceneObjectManager::NotifyEntityDestroyed(Entity entity)
    {
        auto findIter = m_ScriptEntities.find(GetEntityKey(entity));
        if (findIter != m_ScriptEntities.end())
        {
            findIter->second->NotifyDestroyed();
            delete findIter->second;
            m_ScriptEntities.erase(findIter);
        }
    }

    void ScriptSceneObjectManager::NotifyComponentDestroyed(const ComponentBase& component) { NotifyComponentDestroyed(component.InstanceId); }

    void ScriptSceneObjectManager::NotifyComponentDestroyed(uint64_t instanceId)
    {
        auto iterFind = m_ScriptComponents.find(instanceId);
        if (iterFind != m_ScriptComponents.end())
        {
            iterFind->second->NotifyDestroyed();
            delete iterFind->second;
            m_ScriptComponents.erase(iterFind);
        }
    }

    void ScriptSceneObjectManager::DestroySceneObjects(const Scene* scene)
    {
        if (scene == nullptr)
            return;

        for (auto iter = m_ScriptComponents.begin(); iter != m_ScriptComponents.end();)
        {
            ScriptComponentBase* component = iter->second;
            if (component->GetNativeEntity().GetScene() != scene)
            {
                ++iter;
                continue;
            }
            component->NotifyDestroyed();
            delete component;
            iter = m_ScriptComponents.erase(iter);
        }

        for (auto iter = m_ScriptEntities.begin(); iter != m_ScriptEntities.end();)
        {
            ScriptEntity* entity = iter->second;
            if (entity->GetNativeEntity().GetScene() != scene)
            {
                ++iter;
                continue;
            }
            entity->NotifyDestroyed();
            delete entity;
            iter = m_ScriptEntities.erase(iter);
        }
    }

    void ScriptSceneObjectManager::Del()
    {
        CW_ENGINE_INFO("Entities: {0}, components: {1}", m_ScriptEntities.size(), m_ScriptComponents.size());
        for (auto [id, base] : m_ScriptComponents)
        {
            base->NotifyDestroyed();
            delete base;
        }
        m_ScriptComponents.clear();

        for (auto [id, base] : m_ScriptEntities)
        {
            base->NotifyDestroyed();
            delete base;
        }
        m_ScriptEntities.clear();
    }

} // namespace Crowny
