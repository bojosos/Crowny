#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"

#include <box2d/b2_body.h>

namespace Crowny
{
	ScriptRigidbody2D::ScriptRigidbody2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) { }

	void ScriptRigidbody2D::InitRuntimeData()
	{
		MetaData.ScriptClass->AddInternalCall("Internal_GetBodyType", (void*)&Internal_GetBodyType);
		MetaData.ScriptClass->AddInternalCall("Internal_SetBodyType", (void*)&Internal_SetBodyType);
		MetaData.ScriptClass->AddInternalCall("Internal_GetRotation", (void*)&Internal_GetRotation);
		MetaData.ScriptClass->AddInternalCall("Internal_GetPosition", (void*)&Internal_GetPosition);
		MetaData.ScriptClass->AddInternalCall("Internal_GetConstraints", (void*)&Internal_GetConstraints);
		MetaData.ScriptClass->AddInternalCall("Internal_GetMass", (void*)&Internal_GetMass);
		MetaData.ScriptClass->AddInternalCall("Internal_AddForce", (void*)&Internal_AddForce);
		MetaData.ScriptClass->AddInternalCall("Internal_AddForceAt", (void*)&Internal_AddForceAt);
		MetaData.ScriptClass->AddInternalCall("Internal_AddTorque", (void*)&Internal_AddTorque);
	}

	BodyType ScriptRigidbody2D::Internal_GetBodyType(ScriptRigidbody2D* thisPtr)
	{
		return thisPtr->GetComponent().GetBodyType();
	}

	void ScriptRigidbody2D::Internal_SetBodyType(ScriptRigidbody2D* thisPtr, BodyType bodyType)
	{
		thisPtr->GetComponent().SetBodyType(bodyType);
	}

	float ScriptRigidbody2D::Internal_GetMass(ScriptRigidbody2D* thisPtr)
	{
		return thisPtr->GetComponent().GetMass();
	}

	Rigidbody2DConstraints ScriptRigidbody2D::Internal_GetConstraints(ScriptRigidbody2D* thisPtr)
	{
		return thisPtr->GetComponent().GetConstraints();
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

	void ScriptRigidbody2D::Internal_AddForceAt(ScriptRigidbody2D* thisPtr, glm::vec2* offset, glm::vec2* force, ForceMode forceMode)
	{
		if (force->x == 0 && force->y == 0)
			return;
		if (forceMode == ForceMode::Impulse)
			thisPtr->GetComponent().RuntimeBody->ApplyLinearImpulse(b2Vec2(force->x, force->y), b2Vec2(offset->x, offset->y), true);
		else if (forceMode == ForceMode::Force)
			thisPtr->GetComponent().RuntimeBody->ApplyForce(b2Vec2(force->x, force->y), b2Vec2(offset->x, offset->y), true);
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
}