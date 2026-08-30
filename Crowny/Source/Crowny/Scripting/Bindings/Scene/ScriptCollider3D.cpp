#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider3D.h"

namespace Crowny
{
    ScriptCollider3DBase::ScriptCollider3DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    ScriptCollider3D::ScriptCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    ScriptBoxCollider3D::ScriptBoxCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    ScriptSphereCollider3D::ScriptSphereCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    ScriptCapsuleCollider3D::ScriptCapsuleCollider3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider3D::InitRuntimeData() {}

    void ScriptBoxCollider3D::InitRuntimeData() {}

    void ScriptSphereCollider3D::InitRuntimeData() {}

    void ScriptCapsuleCollider3D::InitRuntimeData() {}

} // namespace Crowny
