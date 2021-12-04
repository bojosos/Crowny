#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCamera : public TScriptComponent<ScriptCamera, CameraComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Camera");

        ScriptCamera(MonoObject* instance, Entity entity);

    private:
        static float Internal_GetCameraFov(ScriptCamera* thisPtr);
        static void Internal_SetCameraFov(ScriptCamera* thisPtr, float value);
        static float Internal_GetCameraNearPlane(ScriptCamera* thisPtr);
        static void Internal_SetCameraNearPlane(ScriptCamera* thisPtr, float value);
        static float Internal_GetCameraFarPlane(ScriptCamera* thisPtr);
        static void Internal_SetCameraFarPlane(ScriptCamera* thisPtr, float value);
        static float Internal_GetCameraOrthographicSize(ScriptCamera* thisPtr);
        static void Internal_SetCameraOrthographicSize(ScriptCamera* thisPtr, float value);
        static float Internal_GetCameraAspectRatio(ScriptCamera* thisPtr);
        static void Internal_SetCameraAspectRatio(ScriptCamera* thisPtr, float value);
        static void Internal_GetCameraBackgroundColor(ScriptCamera* thisPtr, glm::vec3* value);
        static void Internal_SetCameraBackgroundColor(ScriptCamera* thisPtr, glm::vec3* value);
        static void Internal_GetCameraViewportRectangle(ScriptCamera* thisPtr, glm::vec4* value);
        static void Internal_SetCameraViewportRectangle(ScriptCamera* thisPtr, glm::vec4* value);
    };
} // namespace Crowny