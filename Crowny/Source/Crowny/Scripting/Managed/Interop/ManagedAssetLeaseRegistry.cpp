#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedAssetLeaseRegistry.h"

#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{
    namespace
    {
        struct ManagedAssetLease
        {
            AssetHandle<Asset> Asset;
            uint32_t Count = 0;
        };

        struct ManagedAssetLeaseOwner
        {
            UnorderedMap<UUID, ManagedAssetLease> Leases;
            Vector<UUID> PendingReleases;
        };

        struct ManagedAssetLeaseState
        {
            Mutex Mutex;
            UnorderedMap<void*, ManagedAssetLeaseOwner> Owners;
        };

        ManagedAssetLeaseState& GetState()
        {
            static ManagedAssetLeaseState state;
            return state;
        }

        void ApplyPendingReleases(ManagedAssetLeaseOwner& owner)
        {
            for (const UUID& assetId : owner.PendingReleases)
            {
                const auto lease = owner.Leases.find(assetId);
                if (lease == owner.Leases.end())
                    continue;
                if (lease->second.Count > 1)
                    --lease->second.Count;
                else
                    owner.Leases.erase(lease);
            }
            owner.PendingReleases.clear();
        }
    } // namespace

    bool ManagedAssetLeaseRegistry::Acquire(void* owner, const UUID& assetId)
    {
        if (owner == nullptr || assetId.Empty())
            return false;

        {
            ManagedAssetLeaseState& state = GetState();
            Lock lock(state.Mutex);
            ManagedAssetLeaseOwner& leases = state.Owners[owner];
            ApplyPendingReleases(leases);
            const auto existing = leases.Leases.find(assetId);
            if (existing != leases.Leases.end())
            {
                ++existing->second.Count;
                return true;
            }
        }

        AssetManager* manager = AssetManager::TryGet();
        return manager != nullptr && Acquire(owner, manager->LoadFromUUID(assetId));
    }

    bool ManagedAssetLeaseRegistry::Acquire(void* owner, const AssetHandle<Asset>& asset)
    {
        if (owner == nullptr || !asset.IsLoaded() || !asset.HasUUID())
            return false;

        ManagedAssetLeaseState& state = GetState();
        Lock lock(state.Mutex);
        ManagedAssetLeaseOwner& leases = state.Owners[owner];
        ApplyPendingReleases(leases);
        ManagedAssetLease& lease = leases.Leases[asset.GetUUID()];
        if (!lease.Asset.IsLoaded())
            lease.Asset = asset;
        ++lease.Count;
        return true;
    }

    void ManagedAssetLeaseRegistry::QueueRelease(void* owner, const UUID& assetId)
    {
        if (owner == nullptr || assetId.Empty())
            return;
        ManagedAssetLeaseState& state = GetState();
        Lock lock(state.Mutex);
        const auto leases = state.Owners.find(owner);
        if (leases != state.Owners.end())
            leases->second.PendingReleases.push_back(assetId);
    }

    void ManagedAssetLeaseRegistry::Drain(void* owner)
    {
        if (owner == nullptr)
            return;
        ManagedAssetLeaseState& state = GetState();
        Lock lock(state.Mutex);
        const auto leases = state.Owners.find(owner);
        if (leases == state.Owners.end())
            return;
        ApplyPendingReleases(leases->second);
        if (leases->second.Leases.empty())
            state.Owners.erase(leases);
    }

    void ManagedAssetLeaseRegistry::ReleaseAll(void* owner)
    {
        if (owner == nullptr)
            return;
        ManagedAssetLeaseState& state = GetState();
        Lock lock(state.Mutex);
        state.Owners.erase(owner);
    }
} // namespace Crowny
