#include "cwpch.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Transform.h"

namespace Crowny
{

	void Transform::InitRuntimeFunctions()
	{
		CWMonoClass* transformClass = CWMonoRuntime::GetAssembly("")->GetClass("Transform");

		transformClass->AddInternalCall("get_position", (void*)Internal_PositionGet);
		transformClass->AddInternalCall("set_position", (void*)Internal_PositionSet);
		transformClass->AddInternalCall("get_localPosition", (void*)Internal_LocalPositionGet);
		transformClass->AddInternalCall("set_localPosition", (void*)Internal_LocalPositionSet);
		transformClass->AddInternalCall("get_eulerAngles", (void*)Internal_EulerRotationGet);
		transformClass->AddInternalCall("set_eulerAngles", (void*)Internal_EulerRotationSet);
		transformClass->AddInternalCall("get_localEulerAngles", (void*)Internal_LocalEulerRotationGet);
		transformClass->AddInternalCall("set_localEulerAngles", (void*)Internal_LocalEulerRotationSet);
		transformClass->AddInternalCall("get_localScale", (void*)Internal_LocalScaleGet);
		transformClass->AddInternalCall("set_localScale", (void*)Internal_LocalScaleSet);
	}

	MonoObject* Transform::Internal_PositionGet(MonoObject* thisptr)
	{
		return 
	}

	void Transform::Internal_PositionSet(MonoObject* thisptr)
	{

	}

	MonoObject* Transform::Internal_LocalPositionGet(MonoObject* thisptr)
	{

	}

	void Transform::Internal_LocalPositionSet(MonoObject* thisptr)
	{

	}

	MonoObject* Transform::Internal_EulerRotationGet(MonoObject* thisptr)
	{

	}

	void Transform::Internal_EulerRotationSet(MonoObject* thisptr)
	{

	}

	MonoObject* Transform::Internal_LocalEulerRotationGet(MonoObject* thisptr)
	{

	}

	void Transform::Internal_LocalEulerRotationSet(MonoObject* thisptr)
	{

	}

	MonoObject Transform::Internal_LocalScaleGet(MonoObject* thisptr)
	{

	}

	void Transform::Internal_LocalScaleSet(MonoObject* thisptr)
	{

	}

}
