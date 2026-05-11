#include "cwpch.h"

#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

namespace Crowny
{

    ScriptObjectManager::~ScriptObjectManager() { ProcessFinalizedObjects(); }

    void ScriptObjectManager::RegisterScriptObject(ScriptObjectBase* instance)
    {
        Lock lock(m_Mutex);
        m_ScriptObjects.insert(instance);
    }

    void ScriptObjectManager::UnregisterScriptObject(ScriptObjectBase* instance)
    {
        Lock lock(m_Mutex);
        m_ScriptObjects.erase(instance);
    }

    void ScriptObjectManager::RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies)
    {
        Map<ScriptObjectBase*, ScriptObjectBackupData> backupData;
        // OnRefreshStarted();

        // TODO: Call scene destroy queued objects, when proper entity deletion is a thing, deleting will be delayed
        // GameObjectManager::Get().DestroyQueuedObjects(); // Wat, why commented

        ProcessFinalizedObjects(false);

        {
            Lock lock(m_Mutex);
            for (const auto& scriptObject : m_ScriptObjects)
                backupData[scriptObject] = scriptObject->BeginRefresh();

            for (const auto& scriptObject : m_ScriptObjects)
                scriptObject->ClearManagedInstance();
        }

        MonoManager::Get().UnloadScriptDomain();

        ProcessFinalizedObjects(true);

        {
            Lock lock(m_Mutex);
            for (const auto& scriptObject : m_ScriptObjects)
                CW_ENGINE_ASSERT(scriptObject->IsPersistent());

            ScriptInfoManager::Get().ClearAssemblyInfo();

            for (const auto& entry : assemblies)
            {
                MonoManager::Get().LoadAssembly(*entry.Filepath, entry.Name);
                ScriptInfoManager::Get().LoadAssemblyInfo(entry.Name);
            }

            Vector<ScriptObjectBase*> scriptObjCopy(m_ScriptObjects.size());
            uint32_t idx = 0;
            for (const auto& scriptObject : m_ScriptObjects)
                scriptObjCopy[idx++] = scriptObject;

            // OnRefreshDomainLoaded();

            for (const auto& scriptObject : scriptObjCopy)
                scriptObject->RestoreManagedInstance();

            for (const auto& scriptObject : scriptObjCopy)
                scriptObject->EndRefresh(backupData[scriptObject]);
        }

        // OnRefreshComplete();
    }

    void ScriptObjectManager::NotifyObjectFinalized(ScriptObjectBase* instance)
    {
        if (instance == nullptr)
            return;
        Lock lock(m_Mutex);
        m_FinalizedObjects[m_FinalizedQueueIdx].push_back(instance);
    }

    void ScriptObjectManager::Update() { ProcessFinalizedObjects(); }

    void ScriptObjectManager::ProcessFinalizedObjects(bool assemblyRefresh)
    {
        uint32_t readQueueIdx = 0;
        {
            Lock lock(m_Mutex);
            readQueueIdx = m_FinalizedQueueIdx;
            m_FinalizedQueueIdx = (m_FinalizedQueueIdx + 1) % 2;
        }

        for (const auto& finalizedObj : m_FinalizedObjects[readQueueIdx])
            finalizedObj->OnManagedInstanceDeleted(assemblyRefresh);

        m_FinalizedObjects[readQueueIdx].clear();
    }
} // namespace Crowny