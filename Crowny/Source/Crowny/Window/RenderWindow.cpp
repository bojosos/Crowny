#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Window/RenderWindow.h"

#include "Platform/Vulkan/VulkanRenderWindow.h"

namespace Crowny
{

    RenderWindow::RenderWindow(const RenderWindowProperties& props) : m_Properties(props) {}

    Ref<RenderWindow> RenderWindow::Create(const RenderWindowProperties& props)
    {
        switch (Renderer::GetAPI())
        {
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanRenderWindow>(props);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny