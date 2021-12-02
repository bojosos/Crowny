#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"

namespace Crowny
{   
    ScriptAudioListener::ScriptAudioListener(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) { }

    void ScriptAudioListener::InitRuntimeData() { }
}