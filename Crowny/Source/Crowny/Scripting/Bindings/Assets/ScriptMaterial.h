#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptMaterial : public TScriptAsset<ScriptMaterial, Material>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Material");
        ScriptMaterial(MonoObject* instance, const AssetHandle<Material>& material);

    private:
    };
} // namespace Crowny
