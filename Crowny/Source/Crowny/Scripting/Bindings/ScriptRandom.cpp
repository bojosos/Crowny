#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Common/Random.h"

namespace Crowny
{
    void ScriptRandom::InitRuntimeFunctions()
    {
        CWMonoClass* randomClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Random");

		randomClass->AddInternalCall("InitState", (void*)&Internal_InitState);
        randomClass->AddInternalCall("Range", (void*)&Internal_Range);
        randomClass->AddInternalCall("get_value", (void*)&Internal_Value);
        randomClass->AddInternalCall("Internal_UnitCircle", (void*)&Internal_UnitCircle);
		randomClass->AddInternalCall("Internal_UnitSphere", (void*)&Internal_UnitSphere);
    }

    void ScriptRandom::Internal_InitState(int32_t seed)
    {
        Random::Seed(seed);
    }

    void ScriptRandom::Internal_UnitCircle(glm::vec2* out)
    {

    }

    void ScriptRandom::Internal_UnitSphere(glm::vec3* out)
    {

    }

    float ScriptRandom::Internal_Range(float min, float max)
    {
        return Random::Float(min, max);
    }

    float ScriptRandom::Internal_Value()
    {
        return Random::Float();
    }

}