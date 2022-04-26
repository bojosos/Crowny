#pragma once

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Audio/AudioClip.h"

namespace Crowny
{
	
	class ScriptAudioClip : public TScriptAsset<ScriptAudioClip, AudioClip>
	{
	public:
		SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioClip");
		ScriptAudioClip(MonoObject* instance, const AssetHandle<AudioClip>& clip);
	private:
		static int Internal_GetBitDepth(ScriptAudioClip* thisPtr);
		static int Internal_GetChannels(ScriptAudioClip* thisPtr);
		static int Internal_GetFrequency(ScriptAudioClip* thisPtr);
		static int Internal_GetSamples(ScriptAudioClip* thisPtr);
		static float Internal_GetLength(ScriptAudioClip* thisPtr);
		static AudioReadMode Internal_GetReadMode(ScriptAudioClip* thisPtr);
		static AudioFormat Internal_GetFormat(ScriptAudioClip* thisPtr);
		static bool Internal_Is3D(ScriptAudioClip* thisPtr);
	};

}