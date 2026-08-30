#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"

namespace Crowny
{
    ScriptPhysicsMaterial3D::ScriptPhysicsMaterial3D(MonoObject* instance, const AssetHandle<PhysicsMaterial3D>& material)
      : TScriptAsset(instance, material)
    {
    }

    void ScriptPhysicsMaterial3D::InitRuntimeData() {}

} // namespace Crowny
