#pragma once

#include "Crowny/RenderAPI/RenderAPI.h"

namespace Crowny
{

    class RenderCommand
    {
    public:
        static void Init() { RenderAPI::Get().Init(); }

        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        {
            // s_RenderAPI->SetViewport(x, y, width, height);
        }

        static void SetClearColor(const glm::vec4& color)
        {
            // s_RenderAPI->SetClearColor(color);
        }

        static void Clear()
        {
            // s_RenderAPI->Clear();
        }

        static void SetDepthTest(bool value)
        {
            // s_RenderAPI->SetDepthTest(value);
        }

        static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t count = -1)
        {
            // s_RenderAPI->DrawIndexed(vertexArray, count);
        }

        static void SwapBuffers()
        {
            // s_RenderAPI->SwapBuffers();
        }
    };
} // namespace Crowny