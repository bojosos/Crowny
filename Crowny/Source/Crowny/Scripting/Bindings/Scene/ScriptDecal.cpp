#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptDecal.h"

namespace Crowny
{
    ScriptDecal::ScriptDecal(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}
    void ScriptDecal::InitRuntimeData() {}
} // namespace Crowny
