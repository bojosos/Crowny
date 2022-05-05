#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

#include <box2d/b2_body.h>

namespace Crowny
{
    ScriptRigidbody2D::ScriptRigidbody2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptRigidbody2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_IsAwake", (void*)&Internal_IsAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBodyType", (void*)&Internal_GetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_SetBodyType", (void*)&Internal_SetBodyType);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConstraints", (void*)&Internal_GetConstraints);
		MetaData.ScriptClass->AddInternalCall("Internal_GetMass", (void*)&Internal_GetMass);
		MetaData.ScriptClass->AddInternalCall("Internal_GetSleepMode", (void*)&Internal_GetSleepMode);
		MetaData.ScriptClass->AddInternalCall("Internal_GetAngularDrag", (void*)&Internal_GetAngularDrag);
		MetaData.ScriptClass->AddInternalCall("Internal_GetLinearDrag", (void*)&Internal_GetLinearDrag);
		MetaData.ScriptClass->AddInternalCall("Internal_GetCollisionDetectionMode", (void*)&Internal_GetCollisionDetectionMode);
		MetaData.ScriptClass->AddInternalCall("Internal_GetLayer", (void*)&Internal_GetLayer);
		
        MetaData.ScriptClass->AddInternalCall("Internal_SetConstraints", (void*)&Internal_SetConstraints);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMass", (void*)&Internal_SetMass);
		MetaData.ScriptClass->AddInternalCall("Internal_SetSleepMode", (void*)&Internal_SetSleepMode);
		MetaData.ScriptClass->AddInternalCall("Internal_SetAngularDrag", (void*)&Internal_SetAngularDrag);
		MetaData.ScriptClass->AddInternalCall("Internal_SetLinearDrag", (void*)&Internal_SetLinearDrag);
		MetaData.ScriptClass->AddInternalCall("Internal_SetCollisionDetectionMode", (void*)&Internal_SetCollisionDetectionMode);
		MetaData.ScriptClass->AddInternalCall("Internal_SetLayer", (void*)&Internal_SetLayer);
		MetaData.ScriptClass->AddInternalCall("Internal_SetSleepMode", (void*)&Internal_SetSleepMode);
		
        MetaData.ScriptClass->AddInternalCall("Internal_GetRotation", (void*)&Internal_GetRotation);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPosition", (void*)&Internal_GetPosition);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForce", (void*)&Internal_AddForce);
        MetaData.ScriptClass->AddInternalCall("Internal_AddForceAt", (void*)&Internal_AddForceAt);
        MetaData.ScriptClass->AddInternalCall("Internal_AddTorque", (void*)&Internal_AddTorque);
    }

	bool ScriptRigidbody2D::Internal_IsAwake(ScriptRigidbody2D* thisPtr)
	{
		return thisPtr->GetComponent().RuntimeBody->IsAwake();
	}
	
    RigidbodyBodyType ScriptRigidbody2D::Internal_GetBodyType(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetBodyType();
    }

    void ScriptRigidbody2D::Internal_SetBodyType(ScriptRigidbody2D* thisPtr, RigidbodyBodyType bodyType)
    {
        thisPtr->GetComponent().SetBodyType(bodyType);
    }

    float ScriptRigidbody2D::Internal_GetMass(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetMass(); }
    bool ScriptRigidbody2D::Internal_GetAutoMass(ScriptRigidbody2D* thisPtr) { return thisPtr->GetComponent().GetAutoMass(); }

    Rigidbody2DConstraints ScriptRigidbody2D::Internal_GetConstraints(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetConstraints();
    }

    float ScriptRigidbody2D::Internal_GetAngularDrag(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetAngularDrag();
    }

    float ScriptRigidbody2D::Internal_GetLinearDrag(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetLinearDrag();
    }

    CollisionDetectionMode2D ScriptRigidbody2D::Internal_GetCollisionDetectionMode(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetCollisionDetectionMode();
    }

    int ScriptRigidbody2D::Internal_GetLayer(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetLayerMask();
    }

    RigidbodySleepMode ScriptRigidbody2D::Internal_GetSleepMode(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetSleepMode();
    }

    void ScriptRigidbody2D::Internal_SetGravityScale(ScriptRigidbody2D* thisPtr, float scale)
    {
        thisPtr->GetComponent().SetGravityScale(scale);
    }

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
        thisPtr->GetComponent().SetLayerMask(layer, thisPtr->GetNativeEntity());
    }

    void ScriptRigidbody2D::Internal_SetSleepMode(ScriptRigidbody2D* thisPtr, RigidbodySleepMode sleepMode)
    {
        thisPtr->GetComponent().SetSleepMode(sleepMode);
    }


    void ScriptRigidbody2D::Internal_SetConstraints(ScriptRigidbody2D* thisPtr, Rigidbody2DConstraints constraints)
    {
		thisPtr->GetComponent().SetConstraints(constraints);
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

    float ScriptRigidbody2D::Internal_GetGravityScale(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().GetGravityScale();
    }

    void ScriptRigidbody2D::Internal_AddForce(ScriptRigidbody2D* thisPtr, glm::vec2* force, ForceMode forceMode)
    {
        if (force->x == 0.0f && force->y == 0.0f)
            return;
        if (forceMode == ForceMode::Impulse)
            thisPtr->GetComponent().RuntimeBody->ApplyLinearImpulseToCenter(b2Vec2(force->x, force->y), true);
        else if (forceMode == ForceMode::Force)
            thisPtr->GetComponent().RuntimeBody->ApplyForceToCenter(b2Vec2(force->x, force->y), true);
    }

    void ScriptRigidbody2D::Internal_AddForceAt(ScriptRigidbody2D* thisPtr, glm::vec2* offset, glm::vec2* force,
                                                ForceMode forceMode)
    {
        if (force->x == 0 && force->y == 0)
            return;
        if (forceMode == ForceMode::Impulse)
            thisPtr->GetComponent().RuntimeBody->ApplyLinearImpulse(b2Vec2(force->x, force->y),
                                                                    b2Vec2(offset->x, offset->y), true);
        else if (forceMode == ForceMode::Force)
            thisPtr->GetComponent().RuntimeBody->ApplyForce(b2Vec2(force->x, force->y), b2Vec2(offset->x, offset->y),
                                                            true);
    }

    void ScriptRigidbody2D::Internal_AddTorque(ScriptRigidbody2D* thisPtr, float torque, ForceMode forceMode)
    {
        if (torque == 0)
            return;
        if (forceMode == ForceMode::Impulse)
            thisPtr->GetComponent().RuntimeBody->ApplyTorque(torque, true);
        else if (forceMode == ForceMode::Force)
            thisPtr->GetComponent().RuntimeBody->ApplyTorque(torque, true);
    }

    float ScriptRigidbody2D::Internal_GetRotation(ScriptRigidbody2D* thisPtr)
    {
        return thisPtr->GetComponent().RuntimeBody->GetAngle();
    }

    void ScriptRigidbody2D::Internal_GetPosition(ScriptRigidbody2D* thisPtr, glm::vec2* outPosition)
    {
        outPosition->x = thisPtr->GetComponent().RuntimeBody->GetPosition().x;
        outPosition->y = thisPtr->GetComponent().RuntimeBody->GetPosition().y;
    }
} // namespace Crowny