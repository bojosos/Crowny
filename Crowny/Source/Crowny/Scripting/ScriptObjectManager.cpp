#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoAssembly.h"
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

    bool ScriptObjectManager::RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies)
    {
        Vector<AssemblyRefreshInfo> previousAssemblies;
        Vector<Path> validationPaths;
        for (const AssemblyRefreshInfo& entry : assemblies)
            validationPaths.push_back(entry.Filepath);

        if (!MonoManager::Get().ValidateAssemblies(validationPaths))
        {
            CW_ENGINE_ERROR("Managed assembly refresh cancelled because validation failed. The current domain remains active.");
            return false;
        }

        for (const AssemblyRefreshInfo& entry : assemblies)
        {
            MonoAssembly* loadedAssembly = MonoManager::Get().GetAssembly(entry.Name);
            if (loadedAssembly != nullptr && loadedAssembly->IsLoaded())
                previousAssemblies.emplace_back(entry.Name, loadedAssembly->GetPath());
        }

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

        ScriptInfoManager::Get().ClearAssemblyInfo();
        MonoManager::Get().UnloadScriptDomain();

        ProcessFinalizedObjects(true);

        bool loaded = true;
        {
            Lock lock(m_Mutex);
            for (const auto& scriptObject : m_ScriptObjects)
                CW_ENGINE_ASSERT(scriptObject->IsPersistent());

            for (const auto& entry : assemblies)
            {
                MonoAssembly& assembly = MonoManager::Get().LoadAssembly(entry.Filepath, entry.Name);
                if (!assembly.IsLoaded())
                {
                    loaded = false;
                    break;
                }
                ScriptInfoManager::Get().LoadAssemblyInfo(entry.Name);
            }

            if (!loaded)
            {
                CW_ENGINE_ERROR("Managed assembly refresh failed after validation. Restoring the previous domain.");
                ScriptInfoManager::Get().ClearAssemblyInfo();
                MonoManager::Get().UnloadScriptDomain();
                for (const AssemblyRefreshInfo& entry : previousAssemblies)
                {
                    MonoAssembly& assembly = MonoManager::Get().LoadAssembly(entry.Filepath, entry.Name);
                    if (!assembly.IsLoaded())
                    {
                        CW_ENGINE_CRITICAL("Could not restore the last working managed assembly {0} from {1}.", entry.Name, entry.Filepath.string());
                        continue;
                    }
                    ScriptInfoManager::Get().LoadAssemblyInfo(entry.Name);
                }
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
        return loaded;
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
