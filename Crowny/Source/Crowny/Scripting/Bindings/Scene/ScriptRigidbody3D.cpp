#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody3D.h"

namespace Crowny
{
    ScriptRigidbody3D::ScriptRigidbody3D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptRigidbody3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetBodyHandle", (void*)&Internal_GetBodyHandle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBodyType", (void*)&Internal_GetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_SetBodyType", (void*)&Internal_SetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMass", (void*)&Internal_GetMass);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMass", (void*)&Internal_SetMass);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoMass", (void*)&Internal_GetAutoMass);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoMass", (void*)&Internal_SetAutoMass);
        MetaData.ScriptClass->AddInternalCall("Internal_GetGravityScale", (void*)&Internal_GetGravityScale);
        MetaData.ScriptClass->AddInternalCall("Internal_SetGravityScale", (void*)&Internal_SetGravityScale);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLinearDamping", (void*)&Internal_GetLinearDamping);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLinearDamping", (void*)&Internal_SetLinearDamping);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAngularDamping", (void*)&Internal_GetAngularDamping);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAngularDamping", (void*)&Internal_SetAngularDamping);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCenterOfMass", (void*)&Internal_GetCenterOfMass);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCenterOfMass", (void*)&Internal_SetCenterOfMass);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAllowSleep", (void*)&Internal_GetAllowSleep);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAllowSleep", (void*)&Internal_SetAllowSleep);
        MetaData.ScriptClass->AddInternalCall("Internal_GetStartAwake", (void*)&Internal_GetStartAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_SetStartAwake", (void*)&Internal_SetStartAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_GetContinuousCollision", (void*)&Internal_GetContinuousCollision);
        MetaData.ScriptClass->AddInternalCall("Internal_SetContinuousCollision", (void*)&Internal_SetContinuousCollision);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConstraints", (void*)&Internal_GetConstraints);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConstraints", (void*)&Internal_SetConstraints);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFilter", (void*)&Internal_GetFilter);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFilter", (void*)&Internal_SetFilter);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLinearVelocity", (void*)&Internal_GetLinearVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLinearVelocity", (void*)&Internal_SetLinearVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAngularVelocity", (void*)&Internal_GetAngularVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAngularVelocity", (void*)&Internal_SetAngularVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_IsAwake", (void*)&Internal_IsAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAwake", (void*)&Internal_SetAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForce", (void*)&Internal_AddForce);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForceAt", (void*)&Internal_AddForceAt);
        MetaData.ScriptClass->AddInternalCall("Internal_AddTorque", (void*)&Internal_AddTorque);
    }

    uint64_t ScriptRigidbody3D::Internal_GetBodyHandle(ScriptRigidbody3D* thisPtr)
    {
        return thisPtr->GetComponent().RuntimeBody.Value;
    }

    int32_t ScriptRigidbody3D::Internal_GetBodyType(ScriptRigidbody3D* thisPtr)
    {
        return static_cast<int32_t>(thisPtr->GetComponent().GetBodyType());
    }

    void ScriptRigidbody3D::Internal_SetBodyType(ScriptRigidbody3D* thisPtr, int32_t value)
    {
        if (value < static_cast<int32_t>(PhysicsBodyType3D::Static) || value > static_cast<int32_t>(PhysicsBodyType3D::Kinematic))
            return;
        thisPtr->GetComponent().SetBodyType(static_cast<PhysicsBodyType3D>(value), thisPtr->GetNativeEntity());
    }

    float ScriptRigidbody3D::Internal_GetMass(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetMass(); }
    void ScriptRigidbody3D::Internal_SetMass(ScriptRigidbody3D* thisPtr, float value)
    {
        thisPtr->GetComponent().SetMass(value, thisPtr->GetNativeEntity());
    }
    bool ScriptRigidbody3D::Internal_GetAutoMass(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetAutoMass(); }
    void ScriptRigidbody3D::Internal_SetAutoMass(ScriptRigidbody3D* thisPtr, bool value)
    {
        thisPtr->GetComponent().SetAutoMass(value, thisPtr->GetNativeEntity());
    }
    float ScriptRigidbody3D::Internal_GetGravityScale(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetGravityScale(); }
    void ScriptRigidbody3D::Internal_SetGravityScale(ScriptRigidbody3D* thisPtr, float value) { thisPtr->GetComponent().SetGravityScale(value); }
    float ScriptRigidbody3D::Internal_GetLinearDamping(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetLinearDamping(); }
    void ScriptRigidbody3D::Internal_SetLinearDamping(ScriptRigidbody3D* thisPtr, float value)
    {
        auto& component = thisPtr->GetComponent();
        component.SetDamping(value, component.GetAngularDamping());
    }
    float ScriptRigidbody3D::Internal_GetAngularDamping(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetAngularDamping(); }
    void ScriptRigidbody3D::Internal_SetAngularDamping(ScriptRigidbody3D* thisPtr, float value)
    {
        auto& component = thisPtr->GetComponent();
        component.SetDamping(component.GetLinearDamping(), value);
    }
    void ScriptRigidbody3D::Internal_GetCenterOfMass(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().GetCenterOfMass();
    }
    void ScriptRigidbody3D::Internal_SetCenterOfMass(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().SetCenterOfMass(*value, thisPtr->GetNativeEntity());
    }
    bool ScriptRigidbody3D::Internal_GetAllowSleep(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetAllowSleep(); }
    void ScriptRigidbody3D::Internal_SetAllowSleep(ScriptRigidbody3D* thisPtr, bool value)
    {
        thisPtr->GetComponent().SetAllowSleep(value, thisPtr->GetNativeEntity());
    }
    bool ScriptRigidbody3D::Internal_GetStartAwake(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetStartAwake(); }
    void ScriptRigidbody3D::Internal_SetStartAwake(ScriptRigidbody3D* thisPtr, bool value)
    {
        thisPtr->GetComponent().SetStartAwake(value, thisPtr->GetNativeEntity());
    }
    bool ScriptRigidbody3D::Internal_GetContinuousCollision(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().GetContinuousCollision(); }
    void ScriptRigidbody3D::Internal_SetContinuousCollision(ScriptRigidbody3D* thisPtr, bool value)
    {
        thisPtr->GetComponent().SetContinuousCollision(value, thisPtr->GetNativeEntity());
    }
    uint32_t ScriptRigidbody3D::Internal_GetConstraints(ScriptRigidbody3D* thisPtr)
    {
        const auto& component = thisPtr->GetComponent();
        return (component.GetLockRotationX() ? 1u : 0u) | (component.GetLockRotationY() ? 2u : 0u) | (component.GetLockRotationZ() ? 4u : 0u);
    }
    void ScriptRigidbody3D::Internal_SetConstraints(ScriptRigidbody3D* thisPtr, uint32_t value)
    {
        thisPtr->GetComponent().SetRotationLocks((value & 1u) != 0, (value & 2u) != 0, (value & 4u) != 0, thisPtr->GetNativeEntity());
    }
    void ScriptRigidbody3D::Internal_GetFilter(ScriptRigidbody3D* thisPtr, PhysicsFilter3D* value) { *value = thisPtr->GetComponent().GetFilter(); }
    void ScriptRigidbody3D::Internal_SetFilter(ScriptRigidbody3D* thisPtr, PhysicsFilter3D* value) { thisPtr->GetComponent().SetFilter(*value); }
    void ScriptRigidbody3D::Internal_GetLinearVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().GetLinearVelocity();
    }
    void ScriptRigidbody3D::Internal_SetLinearVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().SetLinearVelocity(*value);
    }
    void ScriptRigidbody3D::Internal_GetAngularVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().GetAngularVelocity();
    }
    void ScriptRigidbody3D::Internal_SetAngularVelocity(ScriptRigidbody3D* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().SetAngularVelocity(*value);
    }
    bool ScriptRigidbody3D::Internal_IsAwake(ScriptRigidbody3D* thisPtr) { return thisPtr->GetComponent().IsAwake(); }
    void ScriptRigidbody3D::Internal_SetAwake(ScriptRigidbody3D* thisPtr, bool value) { thisPtr->GetComponent().SetAwake(value); }
    void ScriptRigidbody3D::Internal_AddForce(ScriptRigidbody3D* thisPtr, glm::vec3* force, int32_t mode)
    {
        if (mode < static_cast<int32_t>(PhysicsForceMode3D::Force) || mode > static_cast<int32_t>(PhysicsForceMode3D::Acceleration))
            return;
        thisPtr->GetComponent().AddForce(*force, static_cast<PhysicsForceMode3D>(mode));
    }
    void ScriptRigidbody3D::Internal_AddForceAt(ScriptRigidbody3D* thisPtr, glm::vec3* force, glm::vec3* worldPosition, int32_t mode)
    {
        if (mode < static_cast<int32_t>(PhysicsForceMode3D::Force) || mode > static_cast<int32_t>(PhysicsForceMode3D::Acceleration))
            return;
        thisPtr->GetComponent().AddForceAt(*force, *worldPosition, static_cast<PhysicsForceMode3D>(mode));
    }
    void ScriptRigidbody3D::Internal_AddTorque(ScriptRigidbody3D* thisPtr, glm::vec3* torque, int32_t mode)
    {
        if (mode < static_cast<int32_t>(PhysicsForceMode3D::Force) || mode > static_cast<int32_t>(PhysicsForceMode3D::Acceleration))
            return;
        thisPtr->GetComponent().AddTorque(*torque, static_cast<PhysicsForceMode3D>(mode));
    }
} // namespace Crowny
