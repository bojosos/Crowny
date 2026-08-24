#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptRigidbody3D : public TScriptComponent<ScriptRigidbody3D, Rigidbody3DComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Rigidbody3D")

        ScriptRigidbody3D(MonoObject* instance, Entity entity);

    private:
        static uint64_t Internal_GetBodyHandle(ScriptRigidbody3D* thisPtr);
        static int32_t Internal_GetBodyType(ScriptRigidbody3D* thisPtr);
        static void Internal_SetBodyType(ScriptRigidbody3D* thisPtr, int32_t value);
        static float Internal_GetMass(ScriptRigidbody3D* thisPtr);
        static void Internal_SetMass(ScriptRigidbody3D* thisPtr, float value);
        static bool Internal_GetAutoMass(ScriptRigidbody3D* thisPtr);
        static void Internal_SetAutoMass(ScriptRigidbody3D* thisPtr, bool value);
        static float Internal_GetGravityScale(ScriptRigidbody3D* thisPtr);
        static void Internal_SetGravityScale(ScriptRigidbody3D* thisPtr, float value);
        static float Internal_GetLinearDamping(ScriptRigidbody3D* thisPtr);
        static void Internal_SetLinearDamping(ScriptRigidbody3D* thisPtr, float value);
        static float Internal_GetAngularDamping(ScriptRigidbody3D* thisPtr);
        static void Internal_SetAngularDamping(ScriptRigidbody3D* thisPtr, float value);
        static void Internal_GetCenterOfMass(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static void Internal_SetCenterOfMass(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static bool Internal_GetAllowSleep(ScriptRigidbody3D* thisPtr);
        static void Internal_SetAllowSleep(ScriptRigidbody3D* thisPtr, bool value);
        static bool Internal_GetStartAwake(ScriptRigidbody3D* thisPtr);
        static void Internal_SetStartAwake(ScriptRigidbody3D* thisPtr, bool value);
        static bool Internal_GetContinuousCollision(ScriptRigidbody3D* thisPtr);
        static void Internal_SetContinuousCollision(ScriptRigidbody3D* thisPtr, bool value);
        static uint32_t Internal_GetConstraints(ScriptRigidbody3D* thisPtr);
        static void Internal_SetConstraints(ScriptRigidbody3D* thisPtr, uint32_t value);
        static void Internal_GetFilter(ScriptRigidbody3D* thisPtr, PhysicsFilter3D* value);
        static void Internal_SetFilter(ScriptRigidbody3D* thisPtr, PhysicsFilter3D* value);
        static void Internal_GetLinearVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static void Internal_SetLinearVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static void Internal_GetAngularVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static void Internal_SetAngularVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value);
        static bool Internal_IsAwake(ScriptRigidbody3D* thisPtr);
        static void Internal_SetAwake(ScriptRigidbody3D* thisPtr, bool value);
        static void Internal_AddForce(ScriptRigidbody3D* thisPtr, glm::vec3* force, int32_t mode);
        static void Internal_AddForceAt(ScriptRigidbody3D* thisPtr, glm::vec3* force, glm::vec3* worldPosition, int32_t mode);
        static void Internal_AddTorque(ScriptRigidbody3D* thisPtr, glm::vec3* torque, int32_t mode);
    };
} // namespace Crowny
