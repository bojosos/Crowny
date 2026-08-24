#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Window/RenderWindow.h"

#include "Platform/OpenGL/OpenGLRenderWindow.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"

namespace Crowny
{
    RenderWindowProperties::RenderWindowProperties(const RenderWindowDesc& renderWindowDesc)
    {
        Width = renderWindowDesc.Width;
        Height = renderWindowDesc.Height;
        Samples = renderWindowDesc.Samples;
        VSync = renderWindowDesc.VSync;
        Left = renderWindowDesc.Left;
        Top = renderWindowDesc.Top;
        Mode = renderWindowDesc.Mode;
        if (Mode == WindowMode::Windowed && renderWindowDesc.Fullscreen)
            Mode = renderWindowDesc.ShowBorder ? WindowMode::Fullscreen : WindowMode::BorderlessFullscreen;
        Fullscreen = Mode != WindowMode::Windowed;
        IsHidden = renderWindowDesc.Hidden;
        IsModal = renderWindowDesc.Modal;
        IsMaximized = renderWindowDesc.StartMaximized;
        SwapChainTarget = true;
    }

    RenderWindow::RenderWindow(const RenderWindowDesc& renderWindowDesc) : m_Desc(renderWindowDesc) {}

    Ref<RenderWindow> RenderWindow::Create(const RenderWindowDesc& renderWindowDesc)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<RenderWindow>(new OpenGLRenderWindow(renderWindowDesc));
        case RenderAPI::API::Vulkan:
            return Ref<RenderWindow>(new VulkanRenderWindow(renderWindowDesc));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

    }

} // namespace Crowny
