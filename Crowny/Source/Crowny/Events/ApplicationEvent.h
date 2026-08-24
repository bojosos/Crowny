#pragma once

#include "Crowny/Events/Event.h"

namespace Crowny
{
    class WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height) : m_Width(width), m_Height(height), m_FramebufferWidth(width), m_FramebufferHeight(height)
        {
        }

        WindowResizeEvent(uint32_t width, uint32_t height, uint32_t framebufferWidth, uint32_t framebufferHeight)
          : m_Width(width), m_Height(height), m_FramebufferWidth(framebufferWidth), m_FramebufferHeight(framebufferHeight)
        {
        }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetFramebufferWidth() const { return m_FramebufferWidth; }
        uint32_t GetFramebufferHeight() const { return m_FramebufferHeight; }

        String ToString() const override
        {
            StringStream ss;
            ss << "WindowResizeEvent: window=" << m_Width << 'x' << m_Height << ", framebuffer=" << m_FramebufferWidth << 'x' << m_FramebufferHeight;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        uint32_t m_Width, m_Height;
        uint32_t m_FramebufferWidth, m_FramebufferHeight;
    };

    class WindowMinimizeEvent : public Event
    {
    public:
        explicit WindowMinimizeEvent(bool minimized = true) : m_Minimized(minimized) {}

        bool IsMinimized() const { return m_Minimized; }

        EVENT_CLASS_TYPE(WindowMinimize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        bool m_Minimized;
    };

    class WindowMoveEvent : public Event
    {
    public:
        WindowMoveEvent(int32_t left, int32_t top) : m_Left(left), m_Top(top) {}

        int32_t GetLeft() const { return m_Left; }
        int32_t GetTop() const { return m_Top; }

        String ToString() const override
        {
            StringStream ss;
            ss << "WindowMoveEvent: " << m_Left << ", " << m_Top;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowMove)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        int32_t m_Left, m_Top;
    };

    class WindowFocusEvent : public Event
    {
    public:
        WindowFocusEvent() = default;

        EVENT_CLASS_TYPE(WindowFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowLostFocusEvent : public Event
    {
    public:
        WindowLostFocusEvent() = default;

        EVENT_CLASS_TYPE(WindowLostFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() = default;

        void Cancel() { m_Cancelled = true; }
        bool IsCancelled() const { return m_Cancelled; }

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        bool m_Cancelled = false;
    };

    class WindowFileDropEvent : public Event
    {
    public:
        explicit WindowFileDropEvent(Vector<String> paths) : m_Paths(std::move(paths)) {}

        const Vector<String>& GetPaths() const { return m_Paths; }

        EVENT_CLASS_TYPE(WindowFileDrop)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        Vector<String> m_Paths;
    };

    class WindowContentScaleEvent : public Event
    {
    public:
        WindowContentScaleEvent(float xScale, float yScale) : m_XScale(xScale), m_YScale(yScale) {}

        float GetXScale() const { return m_XScale; }
        float GetYScale() const { return m_YScale; }

        EVENT_CLASS_TYPE(WindowContentScale)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        float m_XScale;
        float m_YScale;
    };

    class AppTickEvent : public Event
    {
    public:
        AppTickEvent() = default;

        EVENT_CLASS_TYPE(AppTick)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class AppUpdateEvent : public Event
    {
    public:
        AppUpdateEvent() = default;

        EVENT_CLASS_TYPE(AppUpdate)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class AppRenderEvent : public Event
    {
    public:
        AppRenderEvent() = default;

        EVENT_CLASS_TYPE(AppRender)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };
} // namespace Crowny
