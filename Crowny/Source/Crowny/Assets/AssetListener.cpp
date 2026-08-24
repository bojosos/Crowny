#include "cwpch.h"

#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{
    AssetListener::AssetListener() { AssetListenerManager::Get().RegisterListener(this); }

    AssetListener::~AssetListener() { AssetListenerManager::Get().UnregisterListener(this); }

    void AssetListener::MarkAssetsDirty() { AssetListenerManager::Get().MarkListenerDirty(this); }

    void AssetListenerManager::RegisterListener(AssetListener* assetListener)
    {
        if (assetListener == nullptr)
            return;
        std::lock_guard<RecursiveMutex> lock(m_Mutex);
        m_DirtyListeners.insert(assetListener);
    }

    void AssetListenerManager::UnregisterListener(AssetListener* assetListener)
    {
        if (assetListener == nullptr)
            return;

        std::lock_guard<RecursiveMutex> lock(m_Mutex);
        m_DirtyListeners.erase(assetListener);
        const auto listenerIter = m_ListenerToAssetMap.find(assetListener);
        if (listenerIter == m_ListenerToAssetMap.end())
            return;

        for (const UUID& uuid : listenerIter->second)
        {
            auto assetIter = m_AssetToListenerMap.find(uuid);
            if (assetIter == m_AssetToListenerMap.end())
                continue;
            auto& listeners = assetIter->second;
            listeners.erase(std::remove(listeners.begin(), listeners.end(), assetListener), listeners.end());
            if (listeners.empty())
                m_AssetToListenerMap.erase(assetIter);
        }
        m_ListenerToAssetMap.erase(listenerIter);
    }

    void AssetListenerManager::MarkListenerDirty(AssetListener* assetListener)
    {
        if (assetListener == nullptr)
            return;
        std::lock_guard<RecursiveMutex> lock(m_Mutex);
        m_DirtyListeners.insert(assetListener);
    }

    void AssetListenerManager::UpdateListeners()
    {
        m_TempListenerBuffer.assign(m_DirtyListeners.begin(), m_DirtyListeners.end());
        m_DirtyListeners.clear();

        for (AssetListener* listener : m_TempListenerBuffer)
        {
            auto existing = m_ListenerToAssetMap.find(listener);
            if (existing != m_ListenerToAssetMap.end())
            {
                for (const UUID& uuid : existing->second)
                {
                    auto assetIter = m_AssetToListenerMap.find(uuid);
                    if (assetIter == m_AssetToListenerMap.end())
                        continue;
                    auto& listeners = assetIter->second;
                    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
                    if (listeners.empty())
                        m_AssetToListenerMap.erase(assetIter);
                }
                existing->second.clear();
            }

            m_TempAssetBuffer.clear();
            listener->GetAssets(m_TempAssetBuffer);
            auto& listenerAssets = m_ListenerToAssetMap[listener];
            for (const AssetHandle<Asset>& asset : m_TempAssetBuffer)
            {
                if (!asset.HasUUID())
                    continue;
                const UUID uuid = asset.GetUUID();
                if (std::find(listenerAssets.begin(), listenerAssets.end(), uuid) != listenerAssets.end())
                    continue;
                listenerAssets.push_back(uuid);
                m_AssetToListenerMap[uuid].push_back(listener);
                if (asset.IsLoaded())
                    m_LoadedAssets[uuid] = asset;
            }
        }
        m_TempAssetBuffer.clear();
        m_TempListenerBuffer.clear();
    }

    void AssetListenerManager::Update()
    {
        Map<UUID, AssetHandle<Asset>> loaded;
        Map<UUID, AssetHandle<Asset>> changed;
        Map<UUID, Vector<AssetListener*>> listeners;
        {
            std::lock_guard<RecursiveMutex> lock(m_Mutex);
            UpdateListeners();
            loaded.swap(m_LoadedAssets);
            changed.swap(m_ChangedAssets);
            for (const auto& [uuid, asset] : loaded)
                listeners[uuid] = m_AssetToListenerMap[uuid];
            for (const auto& [uuid, asset] : changed)
                listeners[uuid] = m_AssetToListenerMap[uuid];
        }

        for (const auto& [uuid, asset] : loaded)
        {
            for (AssetListener* listener : listeners[uuid])
                listener->NotifyAssetLoaded(asset);
        }
        for (const auto& [uuid, asset] : changed)
        {
            for (AssetListener* listener : listeners[uuid])
                listener->NotifyAssetChanged(asset);
        }
    }

    void AssetListenerManager::NotifyListeners(const UUID& uuid)
    {
        if (uuid.Empty() || AssetManager::TryGet() == nullptr)
            return;
        const AssetHandle<Asset> asset = AssetManager::TryGet()->GetAssetHandle(uuid);
        if (!asset.IsLoaded())
            return;

        std::lock_guard<RecursiveMutex> lock(m_Mutex);
        if (m_KnownLoadedAssets.insert(uuid).second)
            OnAssetLoaded(asset);
        else
            OnAssetChanged(asset);
    }

    void AssetListenerManager::OnAssetLoaded(const AssetHandle<Asset>& asset) { m_LoadedAssets[asset.GetUUID()] = asset; }

    void AssetListenerManager::OnAssetChanged(const AssetHandle<Asset>& asset) { m_ChangedAssets[asset.GetUUID()] = asset; }
} // namespace Crowny
