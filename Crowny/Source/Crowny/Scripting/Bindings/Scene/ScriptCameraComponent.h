#pragma once

#include "Crowny/Ecs/Components.h"

#include "Crowny/Renderer/Camera.h"

namespace Crowny
{
    class ScriptCameraComponent
    {

    public:
        static void InitRuntimeFunctions();

    private:
        static MonoObject* Internal_GetCamera(CameraComponent* component);
        static void Internal_SetCamera(CameraComponent* component, Camera* camera);
    };
} // namespace Crowny