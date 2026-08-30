#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

namespace Crowny
{
    ScriptRigidbody2D::ScriptRigidbody2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptRigidbody2D::InitRuntimeData() {}

} // namespace Crowny
