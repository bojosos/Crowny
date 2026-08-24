#include "cwpch.h"

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{

    void AssetHandleBase::Destroy()
    {
        if (m_Data == nullptr || m_Data->m_HasInternalRef.load(std::memory_order_acquire))
            return;

        if (AssetManager::TryGet() != nullptr)
            AssetManager::TryGet()->Release(*this);
        else
            ClearHandleData();
    }

    void AssetHandleBase::SetHandleData(const Ref<Asset>& ptr, const UUID& uuid)
    {
        if (m_Data == nullptr)
            m_Data = CreateRef<AssetHandleData>();
        m_Data->m_Ptr = ptr;
        m_Data->m_UUID = uuid;
        m_Data->m_IsCreated = ptr != nullptr;
    }

    bool AssetHandleBase::BlockUntilLoaded() const { return IsLoaded(); }

    void AssetHandleBase::ClearHandleData()
    {
        if (m_Data == nullptr)
            return;
        m_Data->m_Ptr = nullptr;
        m_Data->m_IsCreated = false;
        m_Data->m_HasInternalRef.store(false, std::memory_order_release);
    }

    void AssetHandleBase::AddInternalRef()
    {
        if (m_Data != nullptr)
            m_Data->m_HasInternalRef.store(true, std::memory_order_release);
    }

    void AssetHandleBase::RemoveInternalRef()
    {
        if (m_Data == nullptr)
            return;
        m_Data->m_HasInternalRef.store(false, std::memory_order_release);
        if (m_Data->m_RefCount.load(std::memory_order_acquire) == 0)
            Destroy();
    }

    void AssetHandleBase::NotifyLoadComplete()
    {
        if (m_Data == nullptr)
            return;

        {
            std::lock_guard<Mutex> lock(m_AssetCreatedMutex);
            m_Data->m_IsCreated = true;
        }
        m_AssetCreatedCondition.notify_all();
    }

    void AssetHandleBase::Release()
    {
        if (AssetManager::TryGet() != nullptr)
            AssetManager::TryGet()->Release(*this);
        else
            ClearHandleData();
    }

    Mutex AssetHandleBase::m_AssetCreatedMutex;
    Signal AssetHandleBase::m_AssetCreatedCondition;
} // namespace Crowny
