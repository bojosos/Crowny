#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    class AssetManager;
    class UIUtils; // This is an Editor class

    struct AssetHandleData : public RefCounted
    {
        Ref<Asset> m_Ptr;
        UUID m_UUID;
        bool m_IsCreated = false;
        std::atomic<uint32_t> m_RefCount{ 0 }; // Counts TAssetHandle<T,false> holders (not the IntrusiveRef count)
        std::atomic<bool> m_HasInternalRef{ false };
    };

    class AssetHandleBase
    {
    public:
        bool IsLoaded(bool checkDependencies = false) const { return m_Data != nullptr && m_Data->m_Ptr != nullptr; }
        bool HasUUID() const { return m_Data != nullptr && m_Data->m_UUID != UUID::EMPTY; }
        bool BlockUntilLoaded() const;
        void Release();
        const UUID& GetUUID() const { return m_Data != nullptr ? m_Data->m_UUID : UUID::EMPTY; }

        const Ref<AssetHandleData>& GetHandleData() const { return m_Data; }

    protected:
        void Destroy();
        void SetHandleData(const Ref<Asset>& ptr, const UUID& uuid);
        void ClearHandleData();
        void AddInternalRef();
        void RemoveInternalRef();
        void NotifyLoadComplete();

        Ref<AssetHandleData> m_Data;

    private:
        friend class AssetManager;

        static Mutex m_AssetCreatedMutex;
        static Signal m_AssetCreatedCondition;
    };

    template <bool Weak> class TAssetHandleBase : public AssetHandleBase
    {
    };

    template <> class TAssetHandleBase<true> : public AssetHandleBase
    {
    protected:
        void AddRef() {}
        void ReleaseRef() {}
    };

    template <> class TAssetHandleBase<false> : public AssetHandleBase
    {
    protected:
        void AddRef()
        {
            if (m_Data != nullptr)
                m_Data->m_RefCount.fetch_add(1, std::memory_order_relaxed);
        }

        void ReleaseRef()
        {
            if (m_Data != nullptr)
            {
                const uint32_t refCount = m_Data->m_RefCount.fetch_sub(1, std::memory_order_release);
                if (refCount == 1)
                {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    Destroy();
                }
            }
        }
    };

    template <typename T, bool Weak> class TAssetHandle : public TAssetHandleBase<Weak>
    {
    public:
        TAssetHandle() = default;
        TAssetHandle(std::nullptr_t) {}

        TAssetHandle(const TAssetHandle& other)
        {
            this->m_Data = other.GetHandleData();
            this->AddRef();
        }

        TAssetHandle(TAssetHandle&& other) = default;

        ~TAssetHandle() { this->ReleaseRef(); }

        operator TAssetHandle<Asset, Weak>() const
        {
            TAssetHandle<Asset, Weak> handle;
            handle.SetHandleData(this->GetHandleData());
            return handle;
        }

        T* operator->() const { return Get(); }

        T& operator*() const { return *Get(); }

        TAssetHandle<T, Weak>& operator=(std::nullptr_t ptr)
        {
            this->ReleaseRef();
            this->m_Data = nullptr;
            return *this;
        }

        TAssetHandle<T, Weak>& operator=(const TAssetHandle<T, Weak>& rhs)
        {
            if (this == &rhs)
                return *this;
            SetHandleData(rhs.GetHandleData());
            return *this;
        }

        TAssetHandle& operator=(TAssetHandle&& other)
        {
            if (this == &other)
                return *this;
            this->ReleaseRef();
            this->m_Data = std::exchange(other.m_Data, nullptr);
            return *this;
        }

        explicit operator bool() const { return this->IsLoaded(); }

        T* Get() const { return this->m_Data != nullptr ? static_cast<T*>(this->m_Data->m_Ptr.Get()) : nullptr; }

        Ref<T> GetInternalPtr() const { return this->m_Data != nullptr ? StaticRefCast<T>(this->m_Data->m_Ptr) : nullptr; }

        bool IsExpired() const
        {
            return this->m_Data == nullptr ||
                   (this->m_Data->m_RefCount.load(std::memory_order_acquire) == 0 && !this->m_Data->m_HasInternalRef.load(std::memory_order_acquire));
        }

        TAssetHandle<T, true> GetWeak() const
        {
            TAssetHandle<T, true> handle;
            handle.SetHandleData(this->GetHandleData());
            return handle;
        }

    protected:
        friend class AssetManager;
        friend class UIUtils;
        template <class _Ty, bool _Weak> friend class TAssetHandle;
        template <class _Ty1, class _Ty2, bool _Weak2, bool _Weak1>
        friend TAssetHandle<_Ty1, _Weak1> static_asset_cast(const TAssetHandle<_Ty2, _Weak2>& other);
        template <class _Ty1, class _Ty2, bool _Weak2> friend TAssetHandle<_Ty1, false> static_asset_cast(const TAssetHandle<_Ty2, _Weak2>& other);

        explicit TAssetHandle(T* ptr, const UUID& uuid) : TAssetHandleBase<Weak>()
        {
            this->m_Data = CreateRef<AssetHandleData>();
            this->AddRef();
            this->SetHandleData(Ref<Asset>(ptr), uuid);
            this->m_Data->m_IsCreated = true;
        }

        TAssetHandle(const UUID& uuid)
        {
            this->m_Data = CreateRef<AssetHandleData>();
            this->m_Data->m_UUID = uuid;
            this->AddRef();
        }

        TAssetHandle(const Ref<T> ptr, const UUID& uuid)
        {
            this->m_Data = CreateRef<AssetHandleData>();
            this->AddRef();
            this->SetHandleData(ptr, uuid);
            this->m_Data->m_IsCreated = true;
        }

        void SetHandleData(const Ref<AssetHandleData>& data)
        {
            this->ReleaseRef();
            this->m_Data = data;
            this->AddRef();
        }

        TAssetHandle<T, false> Lock() const
        {
            TAssetHandle<T, false> handle;
            handle.SetHandleData(this->GetHandleData());

            return handle;
        }

        using AssetHandleBase::SetHandleData;
    };

    template <typename T> using AssetHandle = TAssetHandle<T, false>;

    template <typename T> using WeakAssetHandle = TAssetHandle<T, true>;

    template <class _Ty1, class _Ty2, bool _Weak2, bool _Weak1> TAssetHandle<_Ty1, _Weak1> static_asset_cast(const TAssetHandle<_Ty2, _Weak2>& other)
    {
        TAssetHandle<_Ty1, _Weak1> handle;
        handle.SetHandleData(other.GetHandleData());
        return handle;
    }

    template <class _Ty1, class _Ty2, bool _Weak2> TAssetHandle<_Ty1, false> static_asset_cast(const TAssetHandle<_Ty2, _Weak2>& other)
    {
        TAssetHandle<_Ty1, false> handle;
        handle.SetHandleData(other.GetHandleData());
        return handle;
    }

} // namespace Crowny
