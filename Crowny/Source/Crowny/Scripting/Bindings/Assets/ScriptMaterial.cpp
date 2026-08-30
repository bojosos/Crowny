#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptMaterial.h"

namespace Crowny
{
    ScriptMaterial::ScriptMaterial(MonoObject* instance, const AssetHandle<Material>& material) : TScriptAsset(instance, material) {}

    void ScriptMaterial::InitRuntimeData() {}

} // namespace Crowny
