#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
	class ScriptRigidbody2D: public TScriptComponent<ScriptRigidbody2D, Rigidbody2DComponent>
	{
	public:
		SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Rigidbody2D")

		ScriptRigidbody2D(MonoObject* instance, Entity entity);

	private:
		static RigidbodyBodyType Internal_GetBodyType(ScriptRigidbody2D* thisPtr);
		static float Internal_GetRotation(ScriptRigidbody2D* thisPtr);
		static float Internal_GetGravityScale(ScriptRigidbody2D* thisPtr);
		static void Internal_GetPosition(ScriptRigidbody2D* thisPtr, glm::vec2* outPosition);
		static Rigidbody2DConstraints Internal_GetConstraints(ScriptRigidbody2D* thisPtr);
		static float Internal_GetMass(ScriptRigidbody2D* thisPtr);
		static void Internal_SetBodyType(ScriptRigidbody2D* thisPtr, RigidbodyBodyType bodyType);
		static void Internal_AddForce(ScriptRigidbody2D* thisPtr, glm::vec2* force, ForceMode forceType); 
		static void Internal_AddForceAt(ScriptRigidbody2D* thisPtr, glm::vec2* position, glm::vec2* force, ForceMode forceType);
		static void Internal_AddTorque(ScriptRigidbody2D* thisPtr, float torque, ForceMode forceMode);
	};


}