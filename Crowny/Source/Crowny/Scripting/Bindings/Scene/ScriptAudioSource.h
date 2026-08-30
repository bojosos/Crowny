#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{

    class ScriptAudioSource : public TScriptComponent<ScriptAudioSource, AudioSourceComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioSource")
        ScriptAudioSource(MonoObject* instance, Entity entity);

    private:
    };

} // namespace Crowny
