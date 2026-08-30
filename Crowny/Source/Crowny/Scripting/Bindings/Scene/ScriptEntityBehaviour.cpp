#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/Backends/Mono/MonoBindingRegistry.h"
#include "Crowny/Scripting/Backends/Mono/MonoObjectIdentity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity, const ManagedScript& script)
      : ScriptObject(instance)
    {
        m_Entity = entity;
        const ScriptTypeIdentity& identity = script.GetTypeIdentity();
        m_Assembly = identity.Assembly;
        m_Namespace = identity.Namespace;
        m_TypeName = identity.TypeName;
        m_ScriptInstanceId = script.InstanceId;
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        if (!MonoObjectIdentity::SetComponentEntity(instance, entity.GetUuid()))
            CW_ENGINE_ERROR("Could not bind the managed script entity identity.");
    }

    MonoObject* ScriptEntityBehaviour::CreateManagedInstance(bool construct)
    {
        MonoObject* instance;
        MonoClass* currentClass = MonoManager::Get().FindClass(m_Assembly, m_Namespace, m_TypeName);
        if (currentClass == nullptr)
            instance = MonoBindingRegistry::Get().GetBuiltinTypes().MissingEntityBehaviour->CreateInstance(true);
        else
            instance = currentClass->CreateInstance(construct);

        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        if (!MonoObjectIdentity::SetComponentEntity(instance, m_Entity.GetUuid()))
            CW_ENGINE_ERROR("Could not restore the managed script entity identity.");
        return instance;
    }

    void ScriptEntityBehaviour::ClearManagedInstance() { FreeManagedInstance(); }

    void ScriptEntityBehaviour::OnManagedInstanceDeleted(bool assemblyRefresh)
    {
        m_GCHandle = 0;

        if (!assemblyRefresh && GetNativeEntity())
            ScriptSceneObjectManager::Get().DestroyScriptComponent(this, m_ScriptInstanceId);
    }

    void ScriptEntityBehaviour::NotifyDestroyed()
    {
        MonoObject* instance = GetManagedInstance();
        if (instance != nullptr && MetaData.CachedPtrField != nullptr)
        {
            ScriptEntityBehaviour* cleared = nullptr;
            MetaData.CachedPtrField->Set(instance, &cleared);
        }
        FreeManagedInstance();
    }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny
