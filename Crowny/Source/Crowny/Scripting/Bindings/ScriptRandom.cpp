#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptRandom.h"

#include "Crowny/Common/Random.h"

namespace Crowny
{

    ScriptRandom::ScriptRandom() : ScriptObject() {}

    void ScriptRandom::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("InitState", (void*)&Internal_InitState);
        MetaData.ScriptClass->AddInternalCall("Range", (void*)&Internal_Range);
        MetaData.ScriptClass->AddInternalCall("get_value", (void*)&Internal_Value);
        MetaData.ScriptClass->AddInternalCall("Internal_UnitCircle", (void*)&Internal_UnitCircle);
        MetaData.ScriptClass->AddInternalCall("Internal_UnitSphere", (void*)&Internal_UnitSphere);
    }

    void ScriptRandom::Internal_InitState(int32_t seed) { Random::Seed(seed); }

    void ScriptRandom::Internal_UnitCircle(glm::vec2* out) {}

    void ScriptRandom::Internal_UnitSphere(glm::vec3* out) {}

    float ScriptRandom::Internal_Range(float min, float max) { return Random::Float(min, max); }

    float ScriptRandom::Internal_Value() { return Random::Float(); }

} // namespace Crowny