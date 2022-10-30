#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Window/RenderWindow.h"

#include "Platform/Vulkan/VulkanRenderWindow.h"

namespace Crowny
{
    RenderWindowProperties::RenderWindowProperties(const RenderWindowDesc& renderWindowDesc)
    {
        Width = renderWindowDesc.Width;
        Width = renderWindowDesc.Height;
        VSync = renderWindowDesc.VSync;
        Left = renderWindowDesc.Left;
        Top = renderWindowDesc.Top;
        Fullscreen = renderWindowDesc.Fullscreen;
        IsHidden = renderWindowDesc.Hidden;
        IsModal = renderWindowDesc.Modal;
        SwapChainTarget = true;
    }

    RenderWindow::RenderWindow(const RenderWindowDesc& renderWindowDesc) : m_Desc(renderWindowDesc) {}

    Ref<RenderWindow> RenderWindow::Create(const RenderWindowDesc& renderWindowDesc)
    {
        switch (Renderer::GetAPI())
        {
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanRenderWindow>(renderWindowDesc);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny