#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

	class ScriptAssetManager : public Module<ScriptAssetManager>
	{
	public:
		ScriptAssetManager();
		~ScriptAssetManager();
		
		ScriptAssetBase* CreateScriptAsset(const AssetHandle<Asset>& asset, MonoObject* instance);
		ScriptAssetBase* GetScriptAsset(const UUID& uuid);
		ScriptAssetBase* GetScriptAsset(const AssetHandle<Asset>& asset, bool create = false);

		void DestroyScriptAsset(ScriptAssetBase* asset);
		
	private:
		UnorderedMap<UUID, ScriptAssetBase*> m_ScriptAssets;
	};

}