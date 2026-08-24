#pragma once

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/Shader.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class GpuScene;

    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void OnWindowResize(uint32_t width, uint32_t height);
        static RenderAPI::API GetAPI() { return RenderAPI::GetAPI(); }
        static GpuScene& GetGpuScene();
    };

} // namespace Crowny
