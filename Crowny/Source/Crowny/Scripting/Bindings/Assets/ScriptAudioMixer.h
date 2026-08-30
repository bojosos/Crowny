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
    };

} // namespace Crowny
