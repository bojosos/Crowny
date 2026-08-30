#pragma once

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    class ScriptAudioClip : public TScriptAsset<ScriptAudioClip, AudioClip>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioClip");
        ScriptAudioClip(MonoObject* instance, const AssetHandle<AudioClip>& clip);

    private:
    };

} // namespace Crowny
