#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptAudioListener : public TScriptComponent<ScriptAudioListener, AudioListenerComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioListener")

        ScriptAudioListener(MonoObject* instance, Entity entity);
    };
}