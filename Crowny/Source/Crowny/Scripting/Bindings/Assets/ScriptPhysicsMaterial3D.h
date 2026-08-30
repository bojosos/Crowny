#pragma once

#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptPhysicsMaterial3D : public TScriptAsset<ScriptPhysicsMaterial3D, PhysicsMaterial3D>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "PhysicsMaterial3D");
        ScriptPhysicsMaterial3D(MonoObject* instance, const AssetHandle<PhysicsMaterial3D>& material);

    private:
    };
} // namespace Crowny
