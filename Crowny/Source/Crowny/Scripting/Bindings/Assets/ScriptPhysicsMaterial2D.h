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
        static float Internal_GetDensity(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetDensity(ScriptPhysicsMaterial2D* thisPtr, float value);
        static float Internal_GetFriction(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetFriction(ScriptPhysicsMaterial2D* thisPtr, float value);
        static float Internal_GetRestitution(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetRestitution(ScriptPhysicsMaterial2D* thisPtr, float value);
        static float Internal_GetRestitutionThreshold(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetRestitutionThreshold(ScriptPhysicsMaterial2D* thisPtr, float value);
        static PhysicsCombineMode Internal_GetFrictionCombine(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetFrictionCombine(ScriptPhysicsMaterial2D* thisPtr, PhysicsCombineMode value);
        static PhysicsCombineMode Internal_GetRestitutionCombine(ScriptPhysicsMaterial2D* thisPtr);
        static void Internal_SetRestitutionCombine(ScriptPhysicsMaterial2D* thisPtr, PhysicsCombineMode value);
    };
} // namespace Crowny
