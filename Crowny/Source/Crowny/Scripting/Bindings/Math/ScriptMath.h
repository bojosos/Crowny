#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class ScriptMath : public ScriptObject<ScriptMath>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "MathBindings")
        ScriptMath();
        
    private:
        static void Internal_LookAt(glm::vec3* from, glm::vec3* to, glm::vec3* up, glm::mat4* out);
        static void Internal_Inverse(glm::mat4* in, glm::mat4* out);
        static void Internal_InverseAffine(glm::mat4* in, glm::mat4* out);
        static float Internal_Determinant(glm::mat4* matrix);
    };
}