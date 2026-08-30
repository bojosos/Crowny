#pragma once

#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptPhysicsMaterial2D : public TScriptAsset<ScriptPhysicsMaterial2D, PhysicsMaterial2D>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "PhysicsMaterial2D");
        ScriptPhysicsMaterial2D(MonoObject* instance, const AssetHandle<PhysicsMaterial2D>& material);

    private:
    };
} // namespace Crowny
