#include "cwpch.h"

#include "Crowny/Scripting/ScriptObjectManager.h"

namespace Crowny
{

    ScriptObjectManager::~ScriptObjectManager() { ProcessFinalizedObjects(); }

    void ScriptObjectManager::RegisterScriptObject(ScriptObjectBase* instance) { m_ScriptObjects.insert(instance); }

    void ScriptObjectManager::UnregisterScriptObject(ScriptObjectBase* instance) { m_ScriptObjects.erase(instance); }

    void ScriptObjectManager::RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies)
    {
        Map<ScriptObjectBase*, ScriptObjectBackupData> backupData;
        // OnRefreshStarted();

        // GameObjectManager::Get().DestroyQueuedObjects();
        ProcessFinalizedObjects(false);

        for (auto& scriptObject : m_ScriptObjects)
            backupData[scriptObject] = scriptObject->BeginRefresh();

        for (auto& scriptObject : m_ScriptObjects)
            scriptObject->ClearManagedInstance();

        MonoManager::Get().UnloadScriptDomain();

        ProcessFinalizedObjects(true);
        for (auto& scriptObject : m_ScriptObjects)
            CW_ENGINE_ERROR(scriptObject->IsPersistent());

        // ScriptAssemblyManager::Get().ClearAssemblyInfo();

        for (auto& entry : assemblies)
        {
            MonoManager::Get().LoadAssembly(*entry.Filepath, entry.Name);
            // ScriptAssemblyManager::Get().LoadAssemblyInfo(entry.Name, *entry.TypeMappings);

            Vector<ScriptObjectBase*> scriptObjCopy(m_ScriptObjects.size());
            uint32_t idx = 0;
            for (auto& scriptObject : m_ScriptObjects)
                scriptObjCopy[idx++] = scriptObject;

            // OnRefreshDomainLoaded();

            for (auto& scriptObject : scriptObjCopy)
                scriptObject->EndRefresh(backupData[scriptObject]);

            // OnRefreshComplete();
        }
    }

    void ScriptObjectManager::NotifyObjectFinalized(ScriptObjectBase* instance)
    {
        CW_ENGINE_ASSERT(instance != nullptr);
        m_FinalizedObjects[m_FinalizedQueueIdx].push_back(instance);
    }

    void ScriptObjectManager::Update() { ProcessFinalizedObjects(); }

    void ScriptObjectManager::ProcessFinalizedObjects(bool assemblyRefresh)
    {
        uint32_t readQueueIdx = 0;
        readQueueIdx = m_FinalizedQueueIdx;
        m_FinalizedQueueIdx = (m_FinalizedQueueIdx + 1) % 2;

        for (auto& finalizedObj : m_FinalizedObjects[readQueueIdx])
            finalizedObj->OnManagedInstanceDeleted(assemblyRefresh);

        m_FinalizedObjects[readQueueIdx].clear();
    }
} // namespace Crowny