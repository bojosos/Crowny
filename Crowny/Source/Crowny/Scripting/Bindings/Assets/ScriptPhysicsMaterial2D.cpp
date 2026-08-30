#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial2D.h"

namespace Crowny
{
    ScriptPhysicsMaterial2D::ScriptPhysicsMaterial2D(MonoObject* instance, const AssetHandle<PhysicsMaterial2D>& material)
      : TScriptAsset(instance, material)
    {
    }

    void ScriptPhysicsMaterial2D::InitRuntimeData() {}

} // namespace Crowny
