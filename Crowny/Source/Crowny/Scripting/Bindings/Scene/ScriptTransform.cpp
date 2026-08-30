#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

namespace Crowny
{
    ScriptTransform::ScriptTransform(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptTransform::InitRuntimeData() {}

} // namespace Crowny
