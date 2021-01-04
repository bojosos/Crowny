#pragma once

#include "Crowny/Scene/Scene.h"
#include "Crowny/Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class SceneRenderer
    {
    public:
        static void Init();
        static void OnEditorUpdate(Timestep ts, const EditorCamera& camera);
        static void OnRuntimeUpdate(Timestep ts, const Camera& camera, const glm::mat4& cameraTransform);
        static void OnResize(uint32_t width, uint32_t height);
    };
    
}