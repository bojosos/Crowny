#pragma once

#include "Crowny/Scene/Scene.h"

//#include "Crowny/RenderAPI/Framebuffer.h"
#include "Crowny/Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class SceneRenderer
    {
    public:
        static void Init();
        static void OnEditorUpdate(Timestep ts, const EditorCamera& camera);
        static void OnRuntimeUpdate(Timestep ts);
        static void SetViewportSize(float width, float height);
        //      static Ref<Framebuffer> GetMainFramebuffer();
    };

} // namespace Crowny