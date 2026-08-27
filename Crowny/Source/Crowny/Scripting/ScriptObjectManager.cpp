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

    AssemblyRefreshResult ScriptObjectManager::RefreshAssemblies(const Vector<AssemblyRefreshInfo>& assemblies)
    {
        Vector<AssemblyRefreshInfo> previousAssemblies;
        Vector<Path> validationPaths;
        for (const AssemblyRefreshInfo& entry : assemblies)
            validationPaths.push_back(entry.Filepath);

        if (!MonoManager::Get().ValidateAssemblies(validationPaths))
        {
            CW_ENGINE_ERROR("Managed assembly refresh cancelled because validation failed. The current domain remains active.");
            return { AssemblyRefreshStatus::CurrentDomainKept };
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

        Vector<ScriptObjectBase*> persistentObjects;
        {
            Lock lock(m_Mutex);
            persistentObjects.assign(m_ScriptObjects.begin(), m_ScriptObjects.end());
        }
        // Managed getters and wrapper cleanup may register or unregister native
        // script objects. Never run them while holding the manager mutex.
        for (ScriptObjectBase* scriptObject : persistentObjects)
            backupData[scriptObject] = scriptObject->BeginRefresh();
        for (ScriptObjectBase* scriptObject : persistentObjects)
            scriptObject->ClearManagedInstance();

        ScriptInfoManager::Get().ClearAssemblyInfo();
        MonoManager::Get().UnloadScriptDomain();

        ProcessFinalizedObjects(true);

        bool loaded = true;
        Vector<ScriptObjectBase*> scriptObjCopy;
        {
            Lock lock(m_Mutex);
            for (const auto& scriptObject : m_ScriptObjects)
                CW_ENGINE_ASSERT(scriptObject->IsPersistent());
            scriptObjCopy.assign(m_ScriptObjects.begin(), m_ScriptObjects.end());
        }

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

        bool previousDomainRestored = !previousAssemblies.empty();
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
                    previousDomainRestored = false;
                    break;
                }
                ScriptInfoManager::Get().LoadAssemblyInfo(entry.Name);
            }

            if (!previousDomainRestored)
            {
                ScriptInfoManager::Get().ClearAssemblyInfo();
                MonoManager::Get().UnloadScriptDomain();
                ProcessFinalizedObjects(true);
                return { AssemblyRefreshStatus::PreviousDomainRestoreFailed };
            }
        }

        // OnRefreshDomainLoaded();

        for (ScriptObjectBase* scriptObject : scriptObjCopy)
            scriptObject->RestoreManagedInstance();

        for (ScriptObjectBase* scriptObject : scriptObjCopy)
        {
            const auto backup = backupData.find(scriptObject);
            scriptObject->EndRefresh(backup != backupData.end() ? backup->second : ScriptObjectBackupData{});
        }

        // OnRefreshComplete();
        return { loaded ? AssemblyRefreshStatus::ReplacementLoaded : AssemblyRefreshStatus::PreviousDomainRestored };
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
