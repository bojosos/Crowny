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
        static MonoObject* Internal_Create();
        static float Internal_GetDensity(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetDensity(ScriptPhysicsMaterial3D* thisPtr, float value);
        static float Internal_GetFriction(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetFriction(ScriptPhysicsMaterial3D* thisPtr, float value);
        static float Internal_GetRestitution(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetRestitution(ScriptPhysicsMaterial3D* thisPtr, float value);
        static float Internal_GetRestitutionThreshold(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetRestitutionThreshold(ScriptPhysicsMaterial3D* thisPtr, float value);
        static PhysicsCombineMode Internal_GetFrictionCombine(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetFrictionCombine(ScriptPhysicsMaterial3D* thisPtr, PhysicsCombineMode value);
        static PhysicsCombineMode Internal_GetRestitutionCombine(ScriptPhysicsMaterial3D* thisPtr);
        static void Internal_SetRestitutionCombine(ScriptPhysicsMaterial3D* thisPtr, PhysicsCombineMode value);
    };
} // namespace Crowny
