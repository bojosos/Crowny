#pragma once

#include <mono/metadata/object.h>

namespace Crowny
{
	class Transform
	{
	public:
		static void InitRuntimeFunctions();

	private:
		static MonoObject* Internal_PositionGet(MonoObject* thisptr);
		static void Internal_PositionSet(MonoObject* thisptr);
		static MonoObject* Internal_LocalPositionGet(MonoObject* thisptr);
		static void Internal_LocalPositionSet(MonoObject* thisptr);
		static MonoObject* Internal_EulerRotationGet(MonoObject* thisptr);
		static void Internal_EulerRotationSet(MonoObject* thisptr);
		static MonoObject* Internal_LocalEulerRotationGet(MonoObject* thisptr);
		static void Internal_LocalEulerRotationSet(MonoObject* thisptr);
		static MonoObject Internal_LocalScaleGet(MonoObject* thisptr);
		static void Internal_LocalScaleSet(MonoObject* thisptr);
	};
}