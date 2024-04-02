#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{

    ScriptAssetBase::ScriptAssetBase(MonoObject* instance) : PersistentScriptObjectBase(instance) {}

    ScriptAssetBase::~ScriptAssetBase()
    {
        CW_ENGINE_ASSERT(m_GCHandle == 0, "ScriptAssetBase was not properly disposed of!");
    }

    MonoObject* ScriptAssetBase::GetManagedInstance() const { return MonoUtils::GetObjectFromGCHandle(m_GCHandle); }

    void ScriptAssetBase::SetManagedInstance(MonoObject* instance)
    {
        CW_ENGINE_ASSERT(m_GCHandle == 0 && "Attempting to set a new managed instance without freeing the old one!");
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
    }

    void ScriptAssetBase::FreeManagedInstance()
    {
        if (m_GCHandle != 0)
        {
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

    void ScriptAssetBase::Destroy() { ScriptAssetManager::Get().DestroyScriptAsset(this); }

    ::MonoClass* ScriptAssetBase::GetManagedAssetClass(uint32_t id)
    {
        AssetInfo* info = ScriptInfoManager::Get().GetAssetInfo(id);
        if (info == nullptr)
            return nullptr;
        return info->AssetClass->GetInternalPtr();
    }

    ScriptAsset::ScriptAsset(MonoObject* instance) : ScriptObject(instance) {}

    void ScriptAsset::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetName", (void*)&Internal_GetName);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUUID", (void*)&Internal_GetUUID);
    }

    MonoString* ScriptAsset::Internal_GetName(ScriptAssetBase* thisPtr)
    {
        return MonoUtils::ToMonoString(thisPtr->GetGenericHandle()->GetName());
    }

    void ScriptAsset::Internal_GetUUID(ScriptAssetBase* thisPtr, UUID* outUuid)
    {
        *outUuid = thisPtr->GetGenericHandle().GetUUID();
    }
} // namespace Crowny