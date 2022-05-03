#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{

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
        MonoClass* entityClass = ScriptInfoManager::Get().GetBuiltinClasses().EntityClass;
        MonoObject* instance = entityClass->CreateInstance();
        return CreateScriptEntity(instance, entity);
    }

    ScriptEntity* ScriptSceneObjectManager::CreateScriptEntity(MonoObject* existingInstance, Entity entity)
    {
        ScriptEntity* scriptEntity = GetScriptEntity(entity);
        if (scriptEntity != nullptr)
            CW_ENGINE_ERROR("This object already exists");

        ScriptEntity* nativeInstance = new ScriptEntity(existingInstance, entity);
        m_ScriptEntities[(uint32_t)entity.GetHandle()] = nativeInstance;
        return nativeInstance;
    }

    ScriptEntity* ScriptSceneObjectManager::GetScriptEntity(uint32_t id) const
    {
        auto findIter = m_ScriptEntities.find(id);
        if (findIter != m_ScriptEntities.end())
            return findIter->second;
        return nullptr;
    }

    ScriptEntity* ScriptSceneObjectManager::GetScriptEntity(Entity entity) const
    {
        auto findIter = m_ScriptEntities.find((uint32_t)entity.GetHandle());
        if (findIter != m_ScriptEntities.end())
            return findIter->second;
        return nullptr;
    }

    void ScriptSceneObjectManager::DestroyScriptEntity(ScriptEntity* scriptEntity)
    {
        uint32_t id = (uint32_t)scriptEntity->GetNativeEntity().GetHandle();
        m_ScriptEntities.erase(id);
        delete scriptEntity;
    }

    ScriptComponentBase* ScriptSceneObjectManager::GetScriptComponent(Entity entity, const ComponentBase& component,
                                                                      MonoReflectionType* reflType, bool create)
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

    ScriptComponentBase* ScriptSceneObjectManager::CreateScriptComponent(Entity entity, const ComponentBase& component,
                                                                         MonoReflectionType* reflType)
    {
        ComponentInfo* info = ScriptInfoManager::Get().GetComponentInfo(reflType);
        if (info == nullptr)
            return nullptr;
        ScriptComponentBase* nativeInstance = info->CreateCallback(entity);
        nativeInstance->SetNativeEntity(entity);
        uint64_t instanceId = component.InstanceId;
        m_ScriptComponents[instanceId] = nativeInstance;

        return nativeInstance;
    }

    ScriptEntityBehaviour* ScriptSceneObjectManager::CreateManagedScriptComponent(MonoObject* instance, Entity entity, MonoScript& script)
	{
		ScriptEntityBehaviour* nativeInstance = new ScriptEntityBehaviour(instance, entity);
		m_ScriptComponents[script.InstanceId] = nativeInstance;
		return nativeInstance;
    }

    void ScriptSceneObjectManager::DestroyScriptComponent(ScriptComponentBase* scriptComponent, uint64_t instanceId)
    {
        m_ScriptComponents.erase(instanceId);
        delete scriptComponent;
    }

    void ScriptSceneObjectManager::Del()
    {
        CW_ENGINE_INFO("Entities: {0}, components: {1}", m_ScriptEntities.size(), m_ScriptComponents.size());
        for (auto [id, base] : m_ScriptComponents)
        {
            base->ClearManagedInstance();
        }
        for (auto [id, base] : m_ScriptEntities)
        {
            base->ClearManagedInstance();
        }
    }

} // namespace Crowny