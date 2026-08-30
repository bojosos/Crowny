#pragma once

#include "Crowny/Assets/AssetHandle.h"

namespace Crowny
{
    /// Keeps assets referenced by managed identity wrappers alive without exposing native pointers to C#.
    class ManagedAssetLeaseRegistry
    {
    public:
        static bool Acquire(void* owner, const UUID& assetId);
        static bool Acquire(void* owner, const AssetHandle<Asset>& asset);

        /// May be called by a managed finalizer. Destruction is deferred until Drain or ReleaseAll.
        static void QueueRelease(void* owner, const UUID& assetId);
        static void Drain(void* owner);
        static void ReleaseAll(void* owner);
    };
} // namespace Crowny
