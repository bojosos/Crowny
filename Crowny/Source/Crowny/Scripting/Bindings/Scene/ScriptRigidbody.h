#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptRigidbody2D : public TScriptComponent<ScriptRigidbody2D, Rigidbody2DComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Rigidbody2D")

        ScriptRigidbody2D(MonoObject* instance, Entity entity);

    private:
        static bool Internal_IsAwake(ScriptRigidbody2D* thisPtr);

        static RigidbodyBodyType Internal_GetBodyType(ScriptRigidbody2D* thisPtr);
        static float Internal_GetRotation(ScriptRigidbody2D* thisPtr);
        static float Internal_GetGravityScale(ScriptRigidbody2D* thisPtr);
        static void Internal_GetPosition(ScriptRigidbody2D* thisPtr, glm::vec2* outPosition);
        static float Internal_GetAngularDrag(ScriptRigidbody2D* thisPtr);
        static float Internal_GetLinearDrag(ScriptRigidbody2D* thisPtr);
        static CollisionDetectionMode2D Internal_GetCollisionDetectionMode(ScriptRigidbody2D* thisPtr);
        static int Internal_GetLayer(ScriptRigidbody2D* thisPtr);
        static RigidbodySleepMode Internal_GetSleepMode(ScriptRigidbody2D* thisPtr);
        static Rigidbody2DConstraints Internal_GetConstraints(ScriptRigidbody2D* thisPtr, ScriptRigidbody2D* thisPtr2);
        static float Internal_GetMass(ScriptRigidbody2D* thisPtr);
        static bool Internal_GetAutoMass(ScriptRigidbody2D* thisPtr);
        static void Internal_GetCenterOfMass(ScriptRigidbody2D* thisPtr, glm::vec2* outCenterOfMass);

        static void Internal_SetGravityScale(ScriptRigidbody2D* thisPtr, float scale);
        static void Internal_SetAngularDrag(ScriptRigidbody2D* thisPtr, float angularDrag);
        static void Internal_SetLinearDrag(ScriptRigidbody2D* thisPtr, float linearDrag);
        static void Internal_SetCollisionDetectionMode(ScriptRigidbody2D* thisPtr, CollisionDetectionMode2D mode);
        static void Internal_SetLayer(ScriptRigidbody2D* thisPtr, int layer);
        static void Internal_SetSleepMode(ScriptRigidbody2D* thisPtr, RigidbodySleepMode sleepMode);
        static void Internal_SetConstraints(ScriptRigidbody2D* thisPtr, Rigidbody2DConstraints constraints);
        static void Internal_SetBodyType(ScriptRigidbody2D* thisPtr, RigidbodyBodyType bodyType);
        static void Internal_SetMass(ScriptRigidbody2D* thisPtr, float mass);
        static void Internal_SetAutoMass(ScriptRigidbody2D* thisPtr, bool autoMass);
        static void Internal_SetCenterOfMass(ScriptRigidbody2D* thisPtr, glm::vec2* centerOfMass);

        static void Internal_AddForce(ScriptRigidbody2D* thisPtr, glm::vec2* force, ForceMode forceType);
        static void Internal_AddForceAt(ScriptRigidbody2D* thisPtr, glm::vec2* position, glm::vec2* force,
                                        ForceMode forceType);
        static void Internal_AddTorque(ScriptRigidbody2D* thisPtr, float torque, ForceMode forceMode);
    };

} // namespace Crowny