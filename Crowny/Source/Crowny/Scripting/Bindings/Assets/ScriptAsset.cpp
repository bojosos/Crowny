#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{

    ScriptAssetBase::ScriptAssetBase(MonoObject* instance) : PersistentScriptObjectBase(instance) {}

    ScriptAssetBase::~ScriptAssetBase() { CW_ENGINE_ASSERT(m_GCHandle == 0, "ScriptAssetBase was not properly disposed of!"); }

    MonoObject* ScriptAssetBase::GetManagedInstance() const { return m_GCHandle != 0 ? MonoUtils::GetObjectFromGCHandle(m_GCHandle) : nullptr; }

    void ScriptAssetBase::SetManagedInstance(MonoObject* instance)
    {
        CW_ENGINE_ASSERT(m_GCHandle == 0 && "Attempting to set a new managed instance without freeing the old one!");
        m_GCHandle =
          m_Ownership == ScriptAssetOwnership::ManagedOwned ? MonoUtils::NewWeakGCHandle(instance, true) : MonoUtils::NewGCHandle(instance, false);
    }

    void ScriptAssetBase::SetOwnership(ScriptAssetOwnership ownership)
    {
        if (m_Ownership == ownership)
            return;

        MonoObject* const instance = GetManagedInstance();
        const uint32_t replacementHandle = instance == nullptr                               ? 0
                                           : ownership == ScriptAssetOwnership::ManagedOwned ? MonoUtils::NewWeakGCHandle(instance, true)
                                                                                             : MonoUtils::NewGCHandle(instance, false);
        if (m_GCHandle != 0)
            MonoUtils::FreeGCHandle(m_GCHandle);
        m_Ownership = ownership;
        m_GCHandle = replacementHandle;
    }

    void ScriptAssetBase::FreeManagedInstance()
    {
        if (m_GCHandle != 0)
        {
            MonoObject* const instance = GetManagedInstance();
            const ScriptMeta* const objectMeta = ScriptObjectWrapper::GetMetaData();
            if (instance != nullptr && objectMeta != nullptr && objectMeta->CachedPtrField != nullptr)
            {
                ScriptObjectBase* nullInstance = nullptr;
                objectMeta->CachedPtrField->Set(instance, &nullInstance);
            }
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

    void ScriptAssetBase::Destroy() { ScriptAssetManager::Get().DestroyScriptAsset(this); }

    ::MonoClass* ScriptAssetBase::GetManagedAssetClass(uint32_t id)
    {
        AssetInfo* const info = ScriptInfoManager::Get().GetAssetInfo(id);
        if (info == nullptr)
            return nullptr;
        return info->AssetClass->GetInternalPtr();
    }

    ScriptAsset::ScriptAsset(MonoObject* instance) : ScriptObject(instance) {}

    void ScriptAsset::InitRuntimeData() {}
} // namespace Crowny
