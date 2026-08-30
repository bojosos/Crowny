#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"

namespace Crowny
{
    ScriptCollider2DBase::ScriptCollider2DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    ScriptCollider2D::ScriptCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    ScriptBoxCollider2D::ScriptBoxCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    ScriptCircleCollider2D::ScriptCircleCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider2D::InitRuntimeData() {}

    void ScriptBoxCollider2D::InitRuntimeData() {}

    void ScriptCircleCollider2D::InitRuntimeData() {}

} // namespace Crowny
