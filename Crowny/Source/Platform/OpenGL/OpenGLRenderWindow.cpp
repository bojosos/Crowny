#include "cwpch.h"

#include "Platform/OpenGL/OpenGLRenderWindow.h"

#include <algorithm>
#include <stdexcept>

namespace Crowny
{
    OpenGLRenderWindow::OpenGLRenderWindow(const RenderWindowDesc& renderWindowDesc)
      : RenderWindow(renderWindowDesc), m_Properties(renderWindowDesc)
    {
        WindowDesc windowDesc;
        windowDesc.ClientAPI = WindowClientAPI::OpenGL;
#if defined(CW_MACOSX)
        windowDesc.OpenGLMajorVersion = 4;
        windowDesc.OpenGLMinorVersion = 1;
#else
        windowDesc.OpenGLMajorVersion = 4;
        windowDesc.OpenGLMinorVersion = 5;
#endif
#if defined(CW_DEBUG)
        windowDesc.OpenGLDebugContext = true;
#endif
        windowDesc.ShowTitleBar = renderWindowDesc.ShowTitleBar;
        windowDesc.ShowBorder = renderWindowDesc.ShowBorder;
        windowDesc.AllowResize = renderWindowDesc.AllowResize;
        windowDesc.Fullscreen = renderWindowDesc.Fullscreen;
        windowDesc.Mode = renderWindowDesc.Mode;
        windowDesc.Width = renderWindowDesc.Width;
        windowDesc.Height = renderWindowDesc.Height;
        windowDesc.Hidden = renderWindowDesc.Hidden || renderWindowDesc.HideUntilSwap;
        windowDesc.Left = renderWindowDesc.Left;
        windowDesc.Top = renderWindowDesc.Top;
        windowDesc.Title = renderWindowDesc.Title;
        windowDesc.Modal = renderWindowDesc.Modal;
        windowDesc.MonitorIdx = renderWindowDesc.MonitorIdx;
        windowDesc.StartMaximized = renderWindowDesc.StartMaximized;
        windowDesc.MinWidth = renderWindowDesc.MinWidth;
        windowDesc.MinHeight = renderWindowDesc.MinHeight;
        windowDesc.MaxWidth = renderWindowDesc.MaxWidth;
        windowDesc.MaxHeight = renderWindowDesc.MaxHeight;
        windowDesc.AspectRatioNumerator = renderWindowDesc.AspectRatioNumerator;
        windowDesc.AspectRatioDenominator = renderWindowDesc.AspectRatioDenominator;

        m_ShowOnSwap = renderWindowDesc.HideUntilSwap && !renderWindowDesc.Hidden;
        m_Properties.IsHidden = windowDesc.Hidden;
        m_Window = Window::Create(windowDesc);
        if (!m_Window)
            throw std::runtime_error("Could not create the OpenGL render window");

        m_Context = CreateScope<OpenGLContext>(m_Window->GetNativeWindow());
        m_Context->Init();
        SetVSync(renderWindowDesc.VSync);
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::SwapBuffers(uint32_t syncMask)
    {
        (void)syncMask;
        if (m_ShowOnSwap)
            SetHidden(false);
        m_Context->SwapBuffers();
    }

    glm::vec2 OpenGLRenderWindow::ScreenToWindowPosition(const glm::vec2& screenPos) { return m_Window->ScreenToWindowPosition(screenPos); }

    glm::vec2 OpenGLRenderWindow::WindowToScreenPosition(const glm::vec2& windowPos) { return m_Window->WindowToScreenPosition(windowPos); }

    void OpenGLRenderWindow::Resize(uint32_t width, uint32_t height)
    {
        if (m_Window->GetMode() == WindowMode::Windowed)
            m_Window->Resize(width, height);
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::Move(int32_t left, int32_t top)
    {
        if (m_Window->GetMode() == WindowMode::Windowed)
            m_Window->Move(left, top);
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::Minimize()
    {
        m_Window->Minimize();
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::Maximize()
    {
        m_Window->Maximize();
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::Restore()
    {
        m_Window->Restore();
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::SetFullscreen(uint32_t width, uint32_t height, float refreshRate, uint32_t monitorIdx)
    {
        m_Window->SetFullscreen(width, height, static_cast<uint32_t>(std::max(refreshRate, 0.0f)), monitorIdx);
        m_Desc.Mode = WindowMode::Fullscreen;
        m_Desc.Fullscreen = true;
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::SetBorderlessFullscreen(uint32_t monitorIdx)
    {
        m_Window->SetBorderlessFullscreen(monitorIdx);
        m_Desc.Mode = WindowMode::BorderlessFullscreen;
        m_Desc.Fullscreen = true;
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::SetWindowed(uint32_t width, uint32_t height)
    {
        m_Window->SetWindowed(width, height);
        m_Desc.Mode = WindowMode::Windowed;
        m_Desc.Fullscreen = false;
        SyncWindowProperties();
    }

    void OpenGLRenderWindow::SetHidden(bool hidden)
    {
        m_ShowOnSwap = false;
        m_Window->SetHidden(hidden);
        m_Properties.IsHidden = hidden;
    }

    void OpenGLRenderWindow::SetVSync(bool enabled)
    {
        m_Desc.VSync = enabled;
        m_Properties.VSync = enabled;
        m_Context->SetSwapInterval(enabled ? static_cast<int>(std::max(m_Desc.VsyncInterval, 1U)) : 0);
    }

    void OpenGLRenderWindow::SyncWindowProperties() const
    {
        m_Properties.Width = m_Window->GetFramebufferWidth();
        m_Properties.Height = m_Window->GetFramebufferHeight();
        const glm::ivec2 position = m_Window->GetPosition();
        m_Properties.Left = position.x;
        m_Properties.Top = position.y;
        m_Properties.Mode = m_Window->GetMode();
        m_Properties.Fullscreen = m_Properties.Mode != WindowMode::Windowed;
        m_Properties.IsFocused = m_Window->IsFocused();
        m_Properties.IsHidden = m_Window->IsHidden();
        m_Properties.IsMinimized = m_Window->IsMinimized();
        m_Properties.IsMaximized = m_Window->IsMaximized();
    }

    const RenderTargetProperties& OpenGLRenderWindow::GetProperties() const
    {
        SyncWindowProperties();
        return m_Properties;
    }
} // namespace Crowny
