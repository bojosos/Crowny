#pragma once

#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    class ScriptAudioMixer : public TScriptAsset<ScriptAudioMixer, AudioMixer>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioMixer");
        ScriptAudioMixer(MonoObject* instance, const AssetHandle<AudioMixer>& mixer);

    private:
        static void Internal_SetActive(ScriptAudioMixer* thisPtr);
        static float Internal_GetBusVolume(ScriptAudioMixer* thisPtr, MonoString* name);
        static void Internal_SetBusVolume(ScriptAudioMixer* thisPtr, MonoString* name, float volume);
        static bool Internal_IsBusMuted(ScriptAudioMixer* thisPtr, MonoString* name);
        static void Internal_SetBusMuted(ScriptAudioMixer* thisPtr, MonoString* name, bool muted);
    };

} // namespace Crowny
