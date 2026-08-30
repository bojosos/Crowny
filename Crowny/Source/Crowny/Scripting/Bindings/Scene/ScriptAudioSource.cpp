#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"

namespace Crowny
{
    ScriptAudioSource::ScriptAudioSource(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptAudioSource::InitRuntimeData() {}

} // namespace Crowny
