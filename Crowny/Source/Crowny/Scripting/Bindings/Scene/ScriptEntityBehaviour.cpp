#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity)
      : ScriptObject(instance), m_TypeMissing(false), m_Entity(entity)
    {
		MonoUtils::GetClassName(instance, m_Namespace, m_TypeName);
		m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        entity.GetComponent<MonoScriptComponent>().Scripts[0].OnInitialize(this);
    }

    ScriptObjectBackupData ScriptEntityBehaviour::BeginRefresh()
    {
        ScriptObjectBackupData backupData;
        // backupData.Data = 
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

    void ScriptEntityBehaviour::ClearManagedInstance()
    {
        FreeManagedInstance();
    }

    void ScriptEntityBehaviour::OnManagedInstanceDeleted(bool assemblyRefresh)
    {
        m_GCHandle = 0;

        if (!assemblyRefresh) // Check if my component is destroyed
            ScriptSceneObjectManager::Get().DestroyScriptComponent(this, GetNativeEntity().GetComponent<MonoScriptComponent>().InstanceId);
    }

    void ScriptEntityBehaviour::NotifyDestroyed()
    {
        FreeManagedInstance();
    }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny