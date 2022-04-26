#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"

namespace Crowny
{
	ScriptAudioClip::ScriptAudioClip(MonoObject* instance, const AssetHandle<AudioClip>& clip) : TScriptAsset(instance, clip)
	{
		
	}

	void ScriptAudioClip::InitRuntimeData()
	{
		
	}
}