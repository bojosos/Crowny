#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAudioMixer.h"

namespace Crowny
{
    ScriptAudioMixer::ScriptAudioMixer(MonoObject* instance, const AssetHandle<AudioMixer>& mixer) : TScriptAsset(instance, mixer) {}

    void ScriptAudioMixer::InitRuntimeData() {}

} // namespace Crowny
