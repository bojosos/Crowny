#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptLight.h"

namespace Crowny
{
    ScriptLight::ScriptLight(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptLight::InitRuntimeData() {}

} // namespace Crowny
