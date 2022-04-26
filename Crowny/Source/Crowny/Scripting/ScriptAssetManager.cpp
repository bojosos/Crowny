#include "cwpch.h"

#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{
	
	ScriptAssetManager::ScriptAssetManager()
	{
		
	}

	ScriptAssetManager::~ScriptAssetManager()
	{
		
	}

	ScriptAssetBase* ScriptAssetManager::CreateScriptAsset(const AssetHandle<Asset>& asset, MonoObject* instance)
	{
		const UUID& uuid = asset.GetUUID();
		if (!asset.IsLoaded())
			return nullptr;
		
		AssetInfo* assetInfo = ScriptInfoManager::Get().GetAssetInfo(asset->GetAssetType());
		if (assetInfo == nullptr)
			return nullptr;
		ScriptAssetBase* scriptAsset = assetInfo->CreateCallback(asset, instance);
		m_ScriptAssets[uuid] = scriptAsset;
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
			return CreateScriptAsset(asset, nullptr); // TODO: make nullptr default arg
		return nullptr;
	}

	void ScriptAssetManager::DestroyScriptAsset(ScriptAssetBase* asset)
	{
		AssetHandle<Asset> handle = asset->GetGenericHandle();
		const UUID& uuid = handle.GetUUID();
		delete asset;
		m_ScriptAssets.erase(uuid);
	}

}