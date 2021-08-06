#include "cwpch.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Platform/OpenGL/OpenGLRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

namespace Crowny
{
    RenderAPI::API RenderAPI::s_API = RenderAPI::API::Vulkan;

    uint32_t RenderAPI::VertexCountToPrimitiveCount(DrawMode drawMode, uint32_t elementCount)
    {
        switch (drawMode)
        {
        case DrawMode::POINT_LIST:
            return elementCount;
        case DrawMode::LINE_LIST:
            return elementCount / 2;
        case DrawMode::LINE_STRIP:
            return elementCount - 1;
        case DrawMode::TRIANGLE_LIST:
            return elementCount / 3;
        case DrawMode::TRIANGLE_STRIP:
            return elementCount - 2;
        case DrawMode::TRIANGLE_FAN:
            return elementCount - 2;
        }

        return 0;
    }
} // namespace Crowny