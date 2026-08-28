#include "cwpch.h"

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Input/Input.h"
#include "Platform/Linux/LinuxWindow.h"

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        constexpr uint32_t PrimaryInputDeviceId = 0;

        WindowMode ResolveWindowMode(const WindowDesc& desc)
        {
            if (desc.Mode != WindowMode::Windowed)
                return desc.Mode;
            if (!desc.Fullscreen)
                return WindowMode::Windowed;
            return desc.ShowBorder ? WindowMode::Fullscreen : WindowMode::BorderlessFullscreen;
        }

        int ToGLFWLimit(uint32_t value) { return value == 0 ? GLFW_DONT_CARE : static_cast<int>(std::min(value, static_cast<uint32_t>(INT_MAX))); }

        InputModifiers ToInputModifiers(int modifiers)
        {
            InputModifiers result;
            if ((modifiers & GLFW_MOD_SHIFT) != 0)
                result.Set(InputModifierBits::Shift);
            if ((modifiers & GLFW_MOD_CONTROL) != 0)
                result.Set(InputModifierBits::Control);
            if ((modifiers & GLFW_MOD_ALT) != 0)
                result.Set(InputModifierBits::Alt);
            if ((modifiers & GLFW_MOD_SUPER) != 0)
                result.Set(InputModifierBits::Super);
            if ((modifiers & GLFW_MOD_CAPS_LOCK) != 0)
                result.Set(InputModifierBits::CapsLock);
            if ((modifiers & GLFW_MOD_NUM_LOCK) != 0)
                result.Set(InputModifierBits::NumLock);
            return result;
        }
    } // namespace

    LinuxWindow::LinuxWindow(const WindowDesc& windowDesc)
    {
        Init(windowDesc);
        if (m_Window == nullptr)
            throw std::runtime_error("Failed to create the GLFW window");
    }

    LinuxWindow::~LinuxWindow() { Shutdown(); }

    void LinuxWindow::Init(const WindowDesc& windowDesc)
    {
        m_Desc = windowDesc;
        m_Desc.Width = std::max(1U, m_Desc.Width);
        m_Desc.Height = std::max(1U, m_Desc.Height);
        m_Mode = ResolveWindowMode(m_Desc);
        m_Desc.Mode = m_Mode;
        m_Desc.Fullscreen = m_Mode != WindowMode::Windowed;
        m_Data.Title = m_Desc.Title.empty() ? "Crowny Application" : m_Desc.Title;

        if (!Window::Initialize())
            return;

        glfwDefaultWindowHints();
        if (m_Desc.ClientAPI == WindowClientAPI::OpenGL)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, static_cast<int>(m_Desc.OpenGLMajorVersion));
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, static_cast<int>(m_Desc.OpenGLMinorVersion));
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, m_Desc.OpenGLDebugContext ? GLFW_TRUE : GLFW_FALSE);
#if defined(CW_MACOSX)
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        }
        else
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, m_Desc.AllowResize ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, m_Desc.Hidden ? GLFW_FALSE : GLFW_TRUE);
        glfwWindowHint(GLFW_MAXIMIZED, m_Desc.StartMaximized && m_Mode == WindowMode::Windowed ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, m_Desc.ShowTitleBar && m_Desc.ShowBorder ? GLFW_TRUE : GLFW_FALSE);

        GLFWmonitor* monitor = GetMonitor(m_Desc.MonitorIdx);
        const GLFWvidmode* videoMode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        int createWidth = static_cast<int>(m_Desc.Width);
        int createHeight = static_cast<int>(m_Desc.Height);
        GLFWmonitor* creationMonitor = nullptr;

        if (m_Mode == WindowMode::Fullscreen)
        {
            creationMonitor = monitor;
            if (videoMode != nullptr)
                glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate);
        }
        else if (m_Mode == WindowMode::BorderlessFullscreen && videoMode != nullptr)
        {
            createWidth = videoMode->width;
            createHeight = videoMode->height;
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }

        m_Window = glfwCreateWindow(createWidth, createHeight, m_Data.Title.c_str(), creationMonitor, nullptr);
        if (m_Window == nullptr)
        {
            CW_ENGINE_ERROR("Could not create GLFW window '{}'", m_Data.Title);
            Shutdown();
            return;
        }

        glfwSetWindowUserPointer(m_Window, this);
        InstallCallbacks();
        glfwSetInputMode(m_Window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

        if (m_Mode != WindowMode::Fullscreen)
        {
            int monitorX = 0;
            int monitorY = 0;
            int areaX = 0;
            int areaY = 0;
            int areaWidth = createWidth;
            int areaHeight = createHeight;
            if (monitor != nullptr)
            {
                glfwGetMonitorPos(monitor, &monitorX, &monitorY);
                glfwGetMonitorWorkarea(monitor, &areaX, &areaY, &areaWidth, &areaHeight);
            }

            int left = m_Desc.Left;
            int top = m_Desc.Top;
            if (m_Mode == WindowMode::BorderlessFullscreen)
            {
                left = monitorX;
                top = monitorY;
            }
            else
            {
                if (left == -1)
                    left = areaX + std::max(0, areaWidth - createWidth) / 2;
                else
                    left += areaX;
                if (top == -1)
                    top = areaY + std::max(0, areaHeight - createHeight) / 2;
                else
                    top += areaY;
            }
            glfwSetWindowPos(m_Window, left, top);
        }

        SetSizeLimits(m_Desc.MinWidth, m_Desc.MinHeight, m_Desc.MaxWidth, m_Desc.MaxHeight);
        SetAspectRatio(m_Desc.AspectRatioNumerator, m_Desc.AspectRatioDenominator);
        UpdateDimensions();
        m_Data.LastEventWidth = m_Data.Width;
        m_Data.LastEventHeight = m_Data.Height;
        m_Data.LastEventFramebufferWidth = m_Data.FramebufferWidth;
        m_Data.LastEventFramebufferHeight = m_Data.FramebufferHeight;
        if (m_Mode == WindowMode::Windowed)
            RememberWindowedRect();

        float xScale = 1.0f;
        float yScale = 1.0f;
        glfwGetWindowContentScale(m_Window, &xScale, &yScale);
        m_Data.ContentScaleX = xScale;
        m_Data.ContentScaleY = yScale;

        if (m_Desc.Modal)
            CW_ENGINE_WARN("GLFW does not support application-modal windows without a parent window");

        ApplyCursor();
        CW_ENGINE_INFO("Created {}x{} window '{}'", m_Data.Width, m_Data.Height, m_Data.Title);
    }

    void LinuxWindow::InstallCallbacks()
    {
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* nativeWindow, int width, int height) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            window.m_Data.Width = static_cast<uint32_t>(std::max(0, width));
            window.m_Data.Height = static_cast<uint32_t>(std::max(0, height));
            if (window.m_Mode == WindowMode::Windowed && width > 0 && height > 0)
            {
                window.m_WindowedWidth = static_cast<uint32_t>(width);
                window.m_WindowedHeight = static_cast<uint32_t>(height);
            }
            window.UpdateDimensions();
            window.DispatchResizeIfChanged();
        });

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* nativeWindow, int, int) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            window.UpdateDimensions();
            window.DispatchResizeIfChanged();
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* nativeWindow) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            WindowCloseEvent event;
            window.Dispatch(event);
            if (event.IsCancelled())
                glfwSetWindowShouldClose(nativeWindow, GLFW_FALSE);
        });

        glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* nativeWindow, int iconified) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            WindowMinimizeEvent event(iconified == GLFW_TRUE);
            window.Dispatch(event);
        });

        glfwSetWindowPosCallback(m_Window, [](GLFWwindow* nativeWindow, int left, int top) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            window.m_Data.Left = left;
            window.m_Data.Top = top;
            if (window.m_Mode == WindowMode::Windowed)
            {
                window.m_WindowedLeft = left;
                window.m_WindowedTop = top;
            }
            WindowMoveEvent event(left, top);
            window.Dispatch(event);
        });

        glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* nativeWindow, int focused) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            if (focused == GLFW_TRUE)
            {
                Input::OnFocusChanged(true);
                WindowFocusEvent event;
                window.Dispatch(event);
            }
            else
            {
                Input::OnFocusChanged(false);
                WindowLostFocusEvent event;
                window.Dispatch(event);
            }
        });

        glfwSetWindowMaximizeCallback(m_Window, [](GLFWwindow* nativeWindow, int maximized) {
            if (maximized == GLFW_FALSE)
            {
                auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
                if (window.m_Mode == WindowMode::Windowed)
                    window.RememberWindowedRect();
            }
        });

        glfwSetWindowContentScaleCallback(m_Window, [](GLFWwindow* nativeWindow, float xScale, float yScale) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            window.m_Data.ContentScaleX = xScale;
            window.m_Data.ContentScaleY = yScale;
            WindowContentScaleEvent event(xScale, yScale);
            window.Dispatch(event);
            window.UpdateDimensions();
            window.DispatchResizeIfChanged();
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* nativeWindow, int key, int scanCode, int action, int modifiers) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            if (key < 0)
                return;
            Input::OnKeyState(key, action != GLFW_RELEASE);
            if (action == GLFW_PRESS)
            {
                KeyPressedEvent event(static_cast<KeyCode>(key), 0, scanCode, ToInputModifiers(modifiers), glfwGetTime(), PrimaryInputDeviceId);
                window.Dispatch(event);
            }
            else if (action == GLFW_RELEASE)
            {
                KeyReleasedEvent event(static_cast<KeyCode>(key), scanCode, ToInputModifiers(modifiers), glfwGetTime(), PrimaryInputDeviceId);
                window.Dispatch(event);
            }
            else if (action == GLFW_REPEAT)
            {
                KeyPressedEvent event(static_cast<KeyCode>(key), 1, scanCode, ToInputModifiers(modifiers), glfwGetTime(), PrimaryInputDeviceId);
                window.Dispatch(event);
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* nativeWindow, unsigned int codepoint) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            KeyTypedEvent event(codepoint, glfwGetTime(), PrimaryInputDeviceId);
            window.Dispatch(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* nativeWindow, int button, int action, int modifiers) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            Input::OnMouseButtonState(button, action != GLFW_RELEASE);
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(nativeWindow, &x, &y);
            const glm::vec2 position(static_cast<float>(x), static_cast<float>(y));
            if (action == GLFW_PRESS)
            {
                MouseButtonPressedEvent event(static_cast<MouseCode>(button), position, ToInputModifiers(modifiers), glfwGetTime(),
                                              PrimaryInputDeviceId);
                window.Dispatch(event);
            }
            else if (action == GLFW_RELEASE)
            {
                MouseButtonReleasedEvent event(static_cast<MouseCode>(button), position, ToInputModifiers(modifiers), glfwGetTime(),
                                               PrimaryInputDeviceId);
                window.Dispatch(event);
            }
        });

        glfwSetDropCallback(m_Window, [](GLFWwindow* nativeWindow, int count, const char** paths) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            Vector<String> droppedPaths;
            droppedPaths.reserve(static_cast<size_t>(std::max(0, count)));
            for (int i = 0; i < count; ++i)
                droppedPaths.emplace_back(paths[i]);
            WindowFileDropEvent event(std::move(droppedPaths));
            window.Dispatch(event);
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* nativeWindow, double xOffset, double yOffset) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset), glfwGetTime(), PrimaryInputDeviceId);
            window.Dispatch(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* nativeWindow, double x, double y) {
            auto& window = *static_cast<LinuxWindow*>(glfwGetWindowUserPointer(nativeWindow));
            const glm::vec2 delta = Input::OnMousePosition(static_cast<float>(x), static_cast<float>(y));
            MouseMovedEvent event(static_cast<float>(x), static_cast<float>(y), delta, glfwGetTime(), PrimaryInputDeviceId);
            window.Dispatch(event);
        });
    }

    void LinuxWindow::Dispatch(Event& event)
    {
        if (m_Data.EventCallback)
            m_Data.EventCallback(event);
    }

    void LinuxWindow::UpdateDimensions()
    {
        if (m_Window == nullptr)
            return;

        int width = 0;
        int height = 0;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        int left = 0;
        int top = 0;
        glfwGetWindowSize(m_Window, &width, &height);
        glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
        glfwGetWindowPos(m_Window, &left, &top);
        m_Data.Width = static_cast<uint32_t>(std::max(0, width));
        m_Data.Height = static_cast<uint32_t>(std::max(0, height));
        m_Data.FramebufferWidth = static_cast<uint32_t>(std::max(0, framebufferWidth));
        m_Data.FramebufferHeight = static_cast<uint32_t>(std::max(0, framebufferHeight));
        m_Data.Left = left;
        m_Data.Top = top;
    }

    void LinuxWindow::DispatchResizeIfChanged()
    {
        if (m_Data.Width == m_Data.LastEventWidth && m_Data.Height == m_Data.LastEventHeight &&
            m_Data.FramebufferWidth == m_Data.LastEventFramebufferWidth && m_Data.FramebufferHeight == m_Data.LastEventFramebufferHeight)
            return;

        m_Data.LastEventWidth = m_Data.Width;
        m_Data.LastEventHeight = m_Data.Height;
        m_Data.LastEventFramebufferWidth = m_Data.FramebufferWidth;
        m_Data.LastEventFramebufferHeight = m_Data.FramebufferHeight;
        WindowResizeEvent event(m_Data.Width, m_Data.Height, m_Data.FramebufferWidth, m_Data.FramebufferHeight);
        Dispatch(event);
    }

    void LinuxWindow::RememberWindowedRect()
    {
        if (m_Window == nullptr || IsMinimized() || IsMaximized())
            return;

        int width = 0;
        int height = 0;
        glfwGetWindowPos(m_Window, &m_WindowedLeft, &m_WindowedTop);
        glfwGetWindowSize(m_Window, &width, &height);
        if (width > 0 && height > 0)
        {
            m_WindowedWidth = static_cast<uint32_t>(width);
            m_WindowedHeight = static_cast<uint32_t>(height);
        }
    }

    GLFWmonitor* LinuxWindow::GetMonitor(uint32_t monitorIdx) const
    {
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (monitors == nullptr || monitorCount == 0)
            return nullptr;
        if (monitorIdx >= static_cast<uint32_t>(monitorCount))
        {
            CW_ENGINE_WARN("Monitor index {} is unavailable; using the primary monitor", monitorIdx);
            return glfwGetPrimaryMonitor();
        }
        return monitors[monitorIdx];
    }

    void LinuxWindow::SetTitle(const String& title)
    {
        m_Data.Title = title.empty() ? "Crowny Application" : title;
        if (m_Window != nullptr)
            glfwSetWindowTitle(m_Window, m_Data.Title.c_str());
    }

    GLFWcursor* LinuxWindow::GetOrCreateCursor(Cursor cursor)
    {
        if (cursor == Cursor::NO_CURSOR)
            return nullptr;

        const size_t index = static_cast<size_t>(cursor) - 1;
        CW_ENGINE_ASSERT(index < m_Cursors.size(), "Invalid cursor type");
        if (index >= m_Cursors.size())
            return nullptr;
        if (m_Cursors[index] != nullptr)
            return m_Cursors[index];

        int shape = GLFW_ARROW_CURSOR;
        switch (cursor)
        {
        case Cursor::POINTER:
            shape = GLFW_ARROW_CURSOR;
            break;
        case Cursor::IBEAM:
            shape = GLFW_IBEAM_CURSOR;
            break;
        case Cursor::CROSSHAIR:
            shape = GLFW_CROSSHAIR_CURSOR;
            break;
        case Cursor::HAND:
            shape = GLFW_HAND_CURSOR;
            break;
        case Cursor::HRESIZE:
            shape = GLFW_HRESIZE_CURSOR;
            break;
        case Cursor::VRESIZE:
            shape = GLFW_VRESIZE_CURSOR;
            break;
        case Cursor::STOPSIGN:
            shape = GLFW_NOT_ALLOWED_CURSOR;
            break;
        case Cursor::NO_CURSOR:
            break;
        }

        m_Cursors[index] = glfwCreateStandardCursor(shape);
        return m_Cursors[index];
    }

    void LinuxWindow::ApplyCursor()
    {
        if (m_Window == nullptr)
            return;
        if (m_CursorGrabbed || m_CursorType == Cursor::NO_CURSOR)
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            return;
        }

        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetCursor(m_Window, GetOrCreateCursor(m_CursorType));
    }

    void LinuxWindow::SetCursor(Cursor cursor)
    {
        m_CursorType = cursor;
        ApplyCursor();
    }

    void LinuxWindow::SetCursorGrabbed(bool grabbed)
    {
        const bool restoreDefaultCursor = !grabbed && m_CursorType == Cursor::NO_CURSOR;
        if (m_CursorGrabbed == grabbed && !restoreDefaultCursor)
            return;
        m_CursorGrabbed = grabbed;
        if (restoreDefaultCursor)
            m_CursorType = Cursor::POINTER;
        ApplyCursor();
    }

    bool LinuxWindow::IsHidden() const { return m_Window == nullptr || glfwGetWindowAttrib(m_Window, GLFW_VISIBLE) == GLFW_FALSE; }

    bool LinuxWindow::IsFocused() const { return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE; }

    bool LinuxWindow::IsMinimized() const { return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) == GLFW_TRUE; }

    bool LinuxWindow::IsMaximized() const { return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE; }

    bool LinuxWindow::ShouldClose() const { return m_Window == nullptr || glfwWindowShouldClose(m_Window) == GLFW_TRUE; }

    void LinuxWindow::SetHidden(bool hidden)
    {
        if (m_Window == nullptr || IsHidden() == hidden)
            return;
        if (hidden)
            glfwHideWindow(m_Window);
        else
            glfwShowWindow(m_Window);
    }

    void LinuxWindow::Move(int32_t left, int32_t top)
    {
        if (m_Window != nullptr && m_Mode == WindowMode::Windowed)
            glfwSetWindowPos(m_Window, left, top);
    }

    void LinuxWindow::Resize(uint32_t width, uint32_t height)
    {
        if (m_Window != nullptr && width > 0 && height > 0)
            glfwSetWindowSize(m_Window, static_cast<int>(width), static_cast<int>(height));
    }

    void LinuxWindow::Minimize()
    {
        if (m_Window != nullptr)
            glfwIconifyWindow(m_Window);
    }

    void LinuxWindow::Maximize()
    {
        if (m_Window != nullptr && m_Mode == WindowMode::Windowed)
            glfwMaximizeWindow(m_Window);
    }

    void LinuxWindow::Restore()
    {
        if (m_Window != nullptr)
            glfwRestoreWindow(m_Window);
    }

    void LinuxWindow::SetFullscreen(uint32_t width, uint32_t height, uint32_t refreshRate, uint32_t monitorIdx)
    {
        if (m_Window == nullptr)
            return;
        if (m_Mode == WindowMode::Windowed)
            RememberWindowedRect();

        GLFWmonitor* monitor = GetMonitor(monitorIdx);
        if (monitor == nullptr)
            return;
        const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
        if (videoMode == nullptr)
            return;

        width = width == 0 ? static_cast<uint32_t>(videoMode->width) : width;
        height = height == 0 ? static_cast<uint32_t>(videoMode->height) : height;
        m_Mode = WindowMode::Fullscreen;
        m_Desc.MonitorIdx = monitorIdx;
        m_Desc.Fullscreen = true;
        m_Desc.Mode = m_Mode;
        glfwSetWindowMonitor(m_Window, monitor, 0, 0, static_cast<int>(width), static_cast<int>(height),
                             refreshRate == 0 ? GLFW_DONT_CARE : static_cast<int>(refreshRate));
        UpdateDimensions();
        DispatchResizeIfChanged();
    }

    void LinuxWindow::SetBorderlessFullscreen(uint32_t monitorIdx)
    {
        if (m_Window == nullptr)
            return;
        if (m_Mode == WindowMode::Windowed)
            RememberWindowedRect();

        GLFWmonitor* monitor = GetMonitor(monitorIdx);
        if (monitor == nullptr)
            return;
        const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
        if (videoMode == nullptr)
            return;
        int left = 0;
        int top = 0;
        glfwGetMonitorPos(monitor, &left, &top);

        m_Mode = WindowMode::BorderlessFullscreen;
        m_Desc.MonitorIdx = monitorIdx;
        m_Desc.Fullscreen = true;
        m_Desc.Mode = m_Mode;
        glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(m_Window, nullptr, left, top, videoMode->width, videoMode->height, GLFW_DONT_CARE);
        UpdateDimensions();
        DispatchResizeIfChanged();
    }

    void LinuxWindow::SetWindowed(uint32_t width, uint32_t height)
    {
        if (m_Window == nullptr)
            return;

        width = width == 0 ? m_WindowedWidth : width;
        height = height == 0 ? m_WindowedHeight : height;
        width = std::max(1U, width);
        height = std::max(1U, height);
        m_Mode = WindowMode::Windowed;
        m_Desc.Fullscreen = false;
        m_Desc.Mode = m_Mode;
        glfwSetWindowMonitor(m_Window, nullptr, m_WindowedLeft, m_WindowedTop, static_cast<int>(width), static_cast<int>(height), GLFW_DONT_CARE);
        glfwSetWindowAttrib(m_Window, GLFW_DECORATED, m_Desc.ShowTitleBar && m_Desc.ShowBorder ? GLFW_TRUE : GLFW_FALSE);
        SetSizeLimits(m_Desc.MinWidth, m_Desc.MinHeight, m_Desc.MaxWidth, m_Desc.MaxHeight);
        SetAspectRatio(m_Desc.AspectRatioNumerator, m_Desc.AspectRatioDenominator);
        UpdateDimensions();
        RememberWindowedRect();
        DispatchResizeIfChanged();
    }

    void LinuxWindow::SetSizeLimits(uint32_t minWidth, uint32_t minHeight, uint32_t maxWidth, uint32_t maxHeight)
    {
        m_Desc.MinWidth = minWidth;
        m_Desc.MinHeight = minHeight;
        m_Desc.MaxWidth = maxWidth;
        m_Desc.MaxHeight = maxHeight;
        if (m_Window == nullptr)
            return;

        int minW = ToGLFWLimit(minWidth);
        int minH = ToGLFWLimit(minHeight);
        int maxW = ToGLFWLimit(maxWidth);
        int maxH = ToGLFWLimit(maxHeight);
        if (maxW != GLFW_DONT_CARE && minW != GLFW_DONT_CARE && maxW < minW)
            maxW = minW;
        if (maxH != GLFW_DONT_CARE && minH != GLFW_DONT_CARE && maxH < minH)
            maxH = minH;
        glfwSetWindowSizeLimits(m_Window, minW, minH, maxW, maxH);
    }

    void LinuxWindow::SetAspectRatio(uint32_t numerator, uint32_t denominator)
    {
        m_Desc.AspectRatioNumerator = numerator;
        m_Desc.AspectRatioDenominator = denominator;
        if (m_Window == nullptr)
            return;
        if (numerator == 0 || denominator == 0)
            glfwSetWindowAspectRatio(m_Window, GLFW_DONT_CARE, GLFW_DONT_CARE);
        else
            glfwSetWindowAspectRatio(m_Window, static_cast<int>(numerator), static_cast<int>(denominator));
    }

    void LinuxWindow::RequestClose()
    {
        if (m_Window == nullptr)
            return;
        WindowCloseEvent event;
        Dispatch(event);
        if (!event.IsCancelled())
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    void LinuxWindow::Shutdown()
    {
        for (GLFWcursor*& cursor : m_Cursors)
        {
            if (cursor != nullptr)
            {
                glfwDestroyCursor(cursor);
                cursor = nullptr;
            }
        }

        if (m_Window != nullptr)
        {
            glfwSetWindowUserPointer(m_Window, nullptr);
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
    }

} // namespace Crowny
