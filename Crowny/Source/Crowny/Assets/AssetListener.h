#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Module.h"

namespace Crowny
{

    class AssetListenerManager;

    class AssetListener
    {
    public:
        AssetListener();
        virtual ~AssetListener();

        virtual void GetAssets(Vector<AssetHandle<Asset>>& assets) = 0;
        virtual void NotifyAssetLaoded(const AssetHandle<Asset>& asset) {}
        virtual void NotifyAssetChanged(const AssetHandle<Asset>& asset) {}
        virtual void MarkAssetsDirty();

    protected:
        friend class AssetListenerManager;
    };

    class AssetListenerManager : public Module<AssetListenerManager>
    {
    public:
        void RegisterListener(AssetListener* assetListener);
        void UnregisterListener(AssetListener* assetListener);
        void MarkListenerDirty(AssetListener* assetListener);
        void Update();
        void NotifyListeners(const UUID& uuid);

    private:
        void UpdateListeners();
        void OnAssetLoaded(const AssetHandle<Asset>& asset);
        void OnAssetChanged(const AssetHandle<Asset>& asset);

        RecursiveMutex m_Mutex;
        Set<AssetListener> m_DirtyListeners;
        Map<uint64_t, Vector<AssetListener*>> m_AssetToListenerMap;
        Map<AssetListener*, Vector<uint64_t>> m_ListenerToAssetMap;
        Map<UUID, AssetHandle<Asset>> m_LoadedAssets;
        Map<UUID, AssetHandle<Asset>> m_ChangedAssets;

        Vector<AssetHandle<Asset>> m_TempAssetBuffer;
        Vector<AssetListener*> m_TempListenerBuffer;
    };
} // namespace Crowny