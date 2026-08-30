#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"

namespace Crowny
{
    ScriptCamera::ScriptCamera(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCamera::InitRuntimeData() {}

} // namespace Crowny
