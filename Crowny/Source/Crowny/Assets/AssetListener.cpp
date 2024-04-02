#include "cwpch.h"

#include "Crowny/Assets/AssetListener.h"

namespace Crowny
{
    AssetListener::AssetListener() { AssetListenerManager::Get().RegisterListener(this); }

    AssetListener::~AssetListener() { AssetListenerManager::Get().UnregisterListener(this); }

    void AssetListener::MarkAssetsDirty() { AssetListenerManager::Get().MarkListenerDirty(this); }
} // namespace Crowny