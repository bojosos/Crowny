#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity)
      : ScriptObject(instance), m_TypeMissing(false)
    {
        m_Entity = entity;
        MonoUtils::GetClassName(instance, m_Namespace, m_TypeName);
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);

        MonoScriptComponent& monoScriptComponent = entity.GetComponent<MonoScriptComponent>();
        for (MonoScript& script : monoScriptComponent.Scripts)
        {
            if (script.GetNamespace() == m_Namespace && script.GetTypeName() == m_TypeName)
                script.OnInitialize(this);
        }
    }

    ScriptObjectBackupData ScriptEntityBehaviour::BeginRefresh()
    {
        ScriptObjectBackupData backupData;
        backupData = m_Entity.GetComponent<MonoScriptComponent>().Backup(true);
        return backupData;
    }

    void ScriptEntityBehaviour::EndRefresh(const ScriptObjectBackupData& data)
    {
        m_Entity.GetComponent<MonoScriptComponent>().Restore(data, m_TypeMissing);
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

        // TODO: Also could check for HasComponent<>
        // TODO: Fix this. The GetNativeEntity check will probably leak stuff. Need to find a way to delete the managed
        // instance anyways. However might be impossible with the current setup.
        if (!assemblyRefresh && GetNativeEntity()) // Check if my component is destroyed
            ScriptSceneObjectManager::Get().DestroyScriptComponent(
              this, GetNativeEntity().GetComponent<MonoScriptComponent>().InstanceId);
    }

    void ScriptEntityBehaviour::NotifyDestroyed() { FreeManagedInstance(); }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny