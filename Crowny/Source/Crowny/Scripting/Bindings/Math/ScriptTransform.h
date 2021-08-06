#pragma once

#include "Crowny/Ecs/Components.h"

#include <glm/glm.hpp>
#include <mono/metadata/object.h>

namespace Crowny
{

    class ScriptTransform
    {
    public:
        static void InitRuntimeFunctions();

    private:
        static void Internal_PositionGet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_PositionSet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalPositionGet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalPositionSet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_EulerRotationGet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_EulerRotationSet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalEulerRotationGet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalEulerRotationSet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalScaleGet(TransformComponent* thisptr, glm::vec3* value);
        static void Internal_LocalScaleSet(TransformComponent* thisptr, glm::vec3* value);
    };
} // namespace Crowny