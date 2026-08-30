#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody3D.h"

namespace Crowny
{
    ScriptRigidbody3D::ScriptRigidbody3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptRigidbody3D::InitRuntimeData() {}

} // namespace Crowny
