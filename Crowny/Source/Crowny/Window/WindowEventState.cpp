#include "cwpch.h"

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Window/WindowEventState.h"

namespace Crowny
{
    WindowEventState::WindowEventState(const WindowStateSnapshot& initialState) { Reset(initialState); }

    void WindowEventState::Reset(const WindowStateSnapshot& state)
    {
        m_Current = state;
        m_Published = state;
    }

    void WindowEventState::SetSnapshot(const WindowStateSnapshot& state) { m_Current = state; }

    uint32_t WindowEventState::ClampDimension(int32_t value) { return static_cast<uint32_t>(std::max(0, value)); }

    void WindowEventState::SetWindowSize(int32_t width, int32_t height)
    {
        m_Current.Width = ClampDimension(width);
        m_Current.Height = ClampDimension(height);
    }

    void WindowEventState::SetFramebufferSize(int32_t width, int32_t height)
    {
        m_Current.FramebufferWidth = ClampDimension(width);
        m_Current.FramebufferHeight = ClampDimension(height);
    }

    void WindowEventState::SetPosition(int32_t left, int32_t top)
    {
        m_Current.Left = left;
        m_Current.Top = top;
    }

    void WindowEventState::SetContentScale(float xScale, float yScale)
    {
        m_Current.ContentScaleX = xScale;
        m_Current.ContentScaleY = yScale;
    }

    void WindowEventState::SetFocused(bool focused) { m_Current.Focused = focused; }

    void WindowEventState::SetMinimized(bool minimized) { m_Current.Minimized = minimized; }

    void WindowEventState::SetMaximized(bool maximized) { m_Current.Maximized = maximized; }

    void WindowEventState::Flush(const EventCallbackFn& callback)
    {
        if (m_Current.Focused != m_Published.Focused)
        {
            m_Published.Focused = m_Current.Focused;
            if (callback)
            {
                if (m_Current.Focused)
                {
                    WindowFocusEvent event;
                    callback(event);
                }
                else
                {
                    WindowLostFocusEvent event;
                    callback(event);
                }
            }
        }

        if (m_Current.ContentScaleX != m_Published.ContentScaleX || m_Current.ContentScaleY != m_Published.ContentScaleY)
        {
            m_Published.ContentScaleX = m_Current.ContentScaleX;
            m_Published.ContentScaleY = m_Current.ContentScaleY;
            if (callback)
            {
                WindowContentScaleEvent event(m_Current.ContentScaleX, m_Current.ContentScaleY);
                callback(event);
            }
        }

        if (m_Current.Left != m_Published.Left || m_Current.Top != m_Published.Top)
        {
            m_Published.Left = m_Current.Left;
            m_Published.Top = m_Current.Top;
            if (callback)
            {
                WindowMoveEvent event(m_Current.Left, m_Current.Top);
                callback(event);
            }
        }

        m_Published.Maximized = m_Current.Maximized;

        if (m_Current.Minimized != m_Published.Minimized)
        {
            m_Published.Minimized = m_Current.Minimized;
            if (callback)
            {
                WindowMinimizeEvent event(m_Current.Minimized);
                callback(event);
            }
        }

        if (m_Current.Width != m_Published.Width || m_Current.Height != m_Published.Height ||
            m_Current.FramebufferWidth != m_Published.FramebufferWidth || m_Current.FramebufferHeight != m_Published.FramebufferHeight)
        {
            m_Published.Width = m_Current.Width;
            m_Published.Height = m_Current.Height;
            m_Published.FramebufferWidth = m_Current.FramebufferWidth;
            m_Published.FramebufferHeight = m_Current.FramebufferHeight;
            if (callback)
            {
                WindowResizeEvent event(m_Current.Width, m_Current.Height, m_Current.FramebufferWidth, m_Current.FramebufferHeight);
                callback(event);
            }
        }
    }
} // namespace Crowny
