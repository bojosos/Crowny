#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity, MonoScript& script)
      : ScriptObject(instance), m_TypeMissing(false)
    {
        m_Entity = entity;
        m_Namespace = script.GetNamespace();
        m_TypeName = script.GetTypeName();
        m_ScriptInstanceId = script.InstanceId;
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        script.OnInitialize(this);
    }

    ScriptObjectBackupData ScriptEntityBehaviour::BeginRefresh()
    {
        // Back up only the matching MonoScript for this behaviour (not the whole component)
        MonoScriptComponent& msc = m_Entity.GetComponent<MonoScriptComponent>();
        for (auto& script : msc.Scripts)
        {
            if (script.GetNamespace() == m_Namespace && script.GetTypeName() == m_TypeName)
                return script.Backup();
        }
        return {};
    }

    void ScriptEntityBehaviour::EndRefresh(const ScriptObjectBackupData& data)
    {
        // Restore only the matching MonoScript for this behaviour
        MonoScriptComponent& msc = m_Entity.GetComponent<MonoScriptComponent>();
        for (auto& script : msc.Scripts)
        {
            if (script.GetNamespace() == m_Namespace && script.GetTypeName() == m_TypeName)
            {
                script.Restore(data, m_TypeMissing);
                return;
            }
        }
    }

    MonoObject* ScriptEntityBehaviour::CreateManagedInstance(bool construct)
    {
        Ref<SerializableObjectInfo> currentObjInfo = nullptr;

        MonoObject* instance;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(m_Namespace, m_TypeName, currentObjInfo))
        {
            m_TypeMissing = true;
            instance = ScriptInfoManager::Get().GetBuiltinClasses().MissingEntityBehaviour->CreateInstance(true);
        }
        else
        {
            m_TypeMissing = false;
            instance = currentObjInfo->m_MonoClass->CreateInstance(construct);
        }

        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        return instance;
    }

    void ScriptEntityBehaviour::ClearManagedInstance() { FreeManagedInstance(); }

    void ScriptEntityBehaviour::OnManagedInstanceDeleted(bool assemblyRefresh)
    {
        m_GCHandle = 0;

        if (!assemblyRefresh && GetNativeEntity())
            ScriptSceneObjectManager::Get().DestroyScriptComponent(this, m_ScriptInstanceId);
    }

    void ScriptEntityBehaviour::NotifyDestroyed() { FreeManagedInstance(); }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny
