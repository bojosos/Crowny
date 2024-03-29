#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class ScriptTransform : public TScriptComponent<ScriptTransform, TransformComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Transform")

        ScriptTransform(MonoObject* instance, Entity entity);

    private:
        static void Internal_PositionGet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_PositionSet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalPositionGet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalPositionSet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_EulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_EulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalEulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalEulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalScaleGet(ScriptTransform* thisPtr, glm::vec3* value);
        static void Internal_LocalScaleSet(ScriptTransform* thisPtr, glm::vec3* value);
    };
} // namespace Crowny