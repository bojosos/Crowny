#include "cwpch.h"

#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{

    ScriptAssetManager::ScriptAssetManager() {}

    ScriptAssetManager::~ScriptAssetManager() {}

    ScriptAssetBase* ScriptAssetManager::CreateScriptAsset(const AssetHandle<Asset>& asset, MonoObject* instance)
    {
        if (!asset.IsLoaded() || !asset.HasUUID())
            return nullptr;
        AssetInfo* assetInfo = ScriptInfoManager::Get().GetAssetInfo(asset->GetAssetType());
        if (assetInfo == nullptr)
            return nullptr;
        ScriptAssetBase* scriptAsset = assetInfo->CreateCallback(asset, instance);
        m_ScriptAssets[asset.GetUUID()] = scriptAsset;
        return scriptAsset;
    }

    ScriptAssetBase* ScriptAssetManager::GetScriptAsset(const UUID& uuid)
    {
        auto it = m_ScriptAssets.find(uuid);
        if (it == m_ScriptAssets.end())
            return nullptr;
        return it->second;
    }

    ScriptAssetBase* ScriptAssetManager::GetScriptAsset(const AssetHandle<Asset>& asset, bool create)
    {
        const UUID& uuid = asset.GetUUID();
        ScriptAssetBase* output = GetScriptAsset(uuid);
        if (output == nullptr && create)
            return CreateScriptAsset(asset);
        return output;
    }

    void ScriptAssetManager::DestroyScriptAsset(ScriptAssetBase* asset)
    {
        AssetHandle<Asset> handle = asset->GetGenericHandle();
        const UUID& uuid = handle.GetUUID();
        delete asset;
        m_ScriptAssets.erase(uuid);
    }

} // namespace Crowny