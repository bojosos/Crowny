#include "cwpch.h"

#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

namespace Crowny
{
    ScriptRigidbody2D::ScriptRigidbody2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptRigidbody2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_IsAwake", (void*)&Internal_IsAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAwake", (void*)&Internal_SetAwake);

        MetaData.ScriptClass->AddInternalCall("Internal_GetBodyType", (void*)&Internal_GetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConstraints", (void*)&Internal_GetConstraints);
        MetaData.ScriptClass->AddInternalCall("Internal_GetInterpolationMode", (void*)&Internal_GetInterpolationMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLinearVelocity", (void*)&Internal_GetLinearVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAngularVelocity", (void*)&Internal_GetAngularVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMass", (void*)&Internal_GetMass);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSleepMode", (void*)&Internal_GetSleepMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetGravityScale", (void*)&Internal_GetGravityScale);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAngularDrag", (void*)&Internal_GetAngularDrag);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLinearDrag", (void*)&Internal_GetLinearDrag);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCollisionDetectionMode", (void*)&Internal_GetCollisionDetectionMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLayer", (void*)&Internal_GetLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoMass", (void*)&Internal_GetAutoMass);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCenterOfMass", (void*)&Internal_GetCenterOfMass);

        MetaData.ScriptClass->AddInternalCall("Internal_SetBodyType", (void*)&Internal_SetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConstraints", (void*)&Internal_SetConstraints);
        MetaData.ScriptClass->AddInternalCall("Internal_SetInterpolationMode", (void*)&Internal_SetInterpolationMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLinearVelocity", (void*)&Internal_SetLinearVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAngularVelocity", (void*)&Internal_SetAngularVelocity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMass", (void*)&Internal_SetMass);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSleepMode", (void*)&Internal_SetSleepMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetGravityScale", (void*)&Internal_SetGravityScale);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAngularDrag", (void*)&Internal_SetAngularDrag);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLinearDrag", (void*)&Internal_SetLinearDrag);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCollisionDetectionMode", (void*)&Internal_SetCollisionDetectionMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLayer", (void*)&Internal_SetLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoMass", (void*)&Internal_SetAutoMass);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCenterOfMass", (void*)&Internal_SetCenterOfMass);

        MetaData.ScriptClass->AddInternalCall("Internal_GetRotation", (void*)&Internal_GetRotation);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPosition", (void*)&Internal_GetPosition);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForce", (void*)&Internal_AddForce);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForceAt", (void*)&Internal_AddForceAt);
        MetaData.ScriptClass->AddInternalCall("Internal_AddTorque", (void*)&Internal_AddTorque);
        MetaData.ScriptClass->AddInternalCall("Internal_GetInertia", (void*)&Internal_GetInertia);
        MetaData.ScriptClass->AddInternalCall("Internal_SetInertia", (void*)&Internal_SetInertia);
    }

    bool ScriptRigidbody2D::Internal_IsAwake(ScriptRigidbody2D* thisPtr) { return Physics2D::TryGet()->IsBodyAwake(thisPtr->GetNativeEntity()); }

    void ScriptRigidbody2D::Internal_SetAwake(ScriptRigidbody2D* thisPtr, bool awake)
    {
        Physics2D::TryGet()->SetBodyAwake(thisPtr->GetNativeEntity(), awake);
    }

    RigidbodyBodyType ScriptRigidbody2D::Internal_GetBodyType(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetBodyType(); }

    void ScriptRigidbody2D::Internal_SetBodyType(ScriptRigidbody2D* thisPtr, RigidbodyBodyType bodyType)
    {
        thisPtr->GetComponent().SetBodyType(bodyType);
    }

    float ScriptRigidbody2D::Internal_GetMass(ScriptRigidbody2D* thisPtr) { return Physics2D::TryGet()->GetMass(thisPtr->GetNativeEntity()); }
    bool ScriptRigidbody2D::Internal_GetAutoMass(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetAutoMass(); }
    void ScriptRigidbody2D::Internal_GetCenterOfMass(ScriptRigidbody2D* thisPtr, glm::vec2* outCenterOfMass)
    {
        *outCenterOfMass = Physics2D::TryGet()->GetCenterOfMass(thisPtr->GetNativeEntity());
    }

    Rigidbody2DConstraints ScriptRigidbody2D::Internal_GetConstraints(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetConstraints();
    }

    RigidbodyInterpolation ScriptRigidbody2D::Internal_GetInterpolationMode(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetInterpolationMode();
    }

    void ScriptRigidbody2D::Internal_GetLinearVelocity(ScriptRigidbody2D* thisPtr, glm::vec2* outVelocity)
    {
        if (outVelocity)
            *outVelocity = Physics2D::TryGet()->GetLinearVelocity(thisPtr->GetNativeEntity());
    }

    float ScriptRigidbody2D::Internal_GetAngularVelocity(ScriptRigidbody2D* thisPtr)
    {
        return Physics2D::TryGet()->GetAngularVelocity(thisPtr->GetNativeEntity());
    }

    float ScriptRigidbody2D::Internal_GetAngularDrag(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetAngularDrag(); }

    float ScriptRigidbody2D::Internal_GetLinearDrag(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetLinearDrag(); }

    CollisionDetectionMode2D ScriptRigidbody2D::Internal_GetCollisionDetectionMode(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetCollisionDetectionMode();
    }

    int ScriptRigidbody2D::Internal_GetLayer(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetLayerMask(); }

    RigidbodySleepMode ScriptRigidbody2D::Internal_GetSleepMode(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetSleepMode(); }

    void ScriptRigidbody2D::Internal_SetGravityScale(ScriptRigidbody2D* thisPtr, float scale) { thisPtr->GetComponent().SetGravityScale(scale); }

    void ScriptRigidbody2D::Internal_SetAngularDrag(ScriptRigidbody2D* thisPtr, float angularDrag)
    {
        thisPtr->GetComponent().SetAngularDrag(angularDrag);
    }

    void ScriptRigidbody2D::Internal_SetLinearDrag(ScriptRigidbody2D* thisPtr, float linearDrag)
    {
        thisPtr->GetComponent().SetLinearDrag(linearDrag);
    }

    void ScriptRigidbody2D::Internal_SetCollisionDetectionMode(ScriptRigidbody2D* thisPtr, CollisionDetectionMode2D mode)
    {
        thisPtr->GetComponent().SetCollisionDetectionMode(mode);
    }

    void ScriptRigidbody2D::Internal_SetLayer(ScriptRigidbody2D* thisPtr, int layer)
    {
        if (layer < 0 || layer >= static_cast<int>(Physics2DLayerCount))
            return;
        thisPtr->GetComponent().SetLayerMask(static_cast<uint32_t>(layer), thisPtr->GetNativeEntity());
    }

    void ScriptRigidbody2D::Internal_SetSleepMode(ScriptRigidbody2D* thisPtr, RigidbodySleepMode sleepMode)
    {
        thisPtr->GetComponent().SetSleepMode(sleepMode);
    }

    void ScriptRigidbody2D::Internal_SetConstraints(ScriptRigidbody2D* thisPtr, Rigidbody2DConstraints constraints)
    {
        thisPtr->GetComponent().SetConstraints(constraints);
    }

    void ScriptRigidbody2D::Internal_SetInterpolationMode(ScriptRigidbody2D* thisPtr, RigidbodyInterpolation interpolation)
    {
        thisPtr->GetComponent().SetInterpolationMode(interpolation);
    }

    void ScriptRigidbody2D::Internal_SetLinearVelocity(ScriptRigidbody2D* thisPtr, glm::vec2* velocity)
    {
        if (velocity)
            Physics2D::TryGet()->SetLinearVelocity(thisPtr->GetNativeEntity(), *velocity);
    }

    void ScriptRigidbody2D::Internal_SetAngularVelocity(ScriptRigidbody2D* thisPtr, float velocity)
    {
        Physics2D::TryGet()->SetAngularVelocity(thisPtr->GetNativeEntity(), velocity);
    }

    void ScriptRigidbody2D::Internal_SetMass(ScriptRigidbody2D* thisPtr, float mass)
    {
        if (thisPtr->GetComponent().GetAutoMass())
        {
            CW_ERROR("You cannot set mass if auto mass is enabled");
            return;
        }
        thisPtr->GetComponent().SetMass(mass);
    }

    void ScriptRigidbody2D::Internal_SetAutoMass(ScriptRigidbody2D* thisPtr, bool autoMass)
    {
        thisPtr->GetComponent().SetAutoMass(autoMass, thisPtr->GetNativeEntity());
    }

    void ScriptRigidbody2D::Internal_SetCenterOfMass(ScriptRigidbody2D* thisPtr, glm::vec2* centerOfMass)
    {
        thisPtr->GetComponent().SetCenterOfMass(*centerOfMass);
    }

    float ScriptRigidbody2D::Internal_GetGravityScale(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetGravityScale(); }

    void ScriptRigidbody2D::Internal_AddForce(ScriptRigidbody2D* thisPtr, glm::vec2* force, ForceMode forceMode)
    {
        if (force->x == 0.0f && force->y == 0.0f)
            return;
        Physics2D::TryGet()->AddForce(thisPtr->GetNativeEntity(), *force, forceMode);
    }

    void ScriptRigidbody2D::Internal_AddForceAt(ScriptRigidbody2D* thisPtr, glm::vec2* force, glm::vec2* worldPosition, ForceMode forceMode)
    {
        if (force->x == 0 && force->y == 0)
            return;
        Physics2D::TryGet()->AddForceAt(thisPtr->GetNativeEntity(), *force, *worldPosition, forceMode);
    }

    void ScriptRigidbody2D::Internal_AddTorque(ScriptRigidbody2D* thisPtr, float torque, ForceMode forceMode)
    {
        if (torque == 0)
            return;
        Physics2D::TryGet()->AddTorque(thisPtr->GetNativeEntity(), torque, forceMode);
    }

    float ScriptRigidbody2D::Internal_GetRotation(ScriptRigidbody2D* thisPtr) { return Physics2D::TryGet()->GetRotation(thisPtr->GetNativeEntity()); }

    void ScriptRigidbody2D::Internal_GetPosition(ScriptRigidbody2D* thisPtr, glm::vec2* outPosition)
    {
        *outPosition = Physics2D::TryGet()->GetPosition(thisPtr->GetNativeEntity());
    }

    float ScriptRigidbody2D::Internal_GetInertia(ScriptRigidbody2D* thisPtr) { return Physics2D::TryGet()->GetInertia(thisPtr->GetNativeEntity()); }

    void ScriptRigidbody2D::Internal_SetInertia(ScriptRigidbody2D* thisPtr, float inertia) { thisPtr->GetComponent().SetInertia(inertia); }
} // namespace Crowny
