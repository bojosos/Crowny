#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Math/ScriptMath.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace Crowny
{

    ScriptMath::ScriptMath() : ScriptObject() { }

    void ScriptMath::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_LookAt", (void*)&Internal_LookAt);
        MetaData.ScriptClass->AddInternalCall("Internal_Inverse", (void*)&Internal_Inverse);
        MetaData.ScriptClass->AddInternalCall("Internal_InverseAffine", (void*)&Internal_InverseAffine);
        MetaData.ScriptClass->AddInternalCall("Internal_Determinant", (void*)&Internal_Determinant);
    }

    void ScriptMath::Internal_Inverse(glm::mat4* in, glm::mat4* out)
    {
        *out = glm::inverse(*in);
    }

    void ScriptMath::Internal_InverseAffine(glm::mat4* in, glm::mat4* out)
    {
        *out = glm::affineInverse(*in);
    }

    void ScriptMath::Internal_LookAt(glm::vec3* from, glm::vec3* to, glm::vec3* up, glm::mat4* out)
    {
        *out = glm::lookAt(*from, *to, *up);
    }

    float ScriptMath::Internal_Determinant(glm::mat4* in)
    {
        return glm::determinant(*in);
    }

}