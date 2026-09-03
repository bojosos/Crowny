#pragma once

#include "Crowny/Events/Event.h"

namespace Crowny
{

    enum class WindowMode
    {
        Windowed,
        Fullscreen,
        BorderlessFullscreen
    };

    enum class WindowClientAPI
    {
        None,
        OpenGL
    };

    struct WindowDesc
    {
        String Title = "Crowny Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        int32_t Left = -1;
        int32_t Top = -1;
        WindowMode Mode = WindowMode::Windowed;
        WindowClientAPI ClientAPI = WindowClientAPI::None;

        bool ShowTitleBar = true;
        bool ShowBorder = true;
        bool AllowResize = true;
        // Kept for source compatibility. Mode takes precedence when it is not Windowed.
        bool Fullscreen = false;
        bool Hidden = false;
        bool Modal = false;
        bool StartMaximized = false;
        uint32_t MonitorIdx = 0;

        uint32_t MinWidth = 1;
        uint32_t MinHeight = 1;
        uint32_t MaxWidth = 0;
        uint32_t MaxHeight = 0;
        uint32_t AspectRatioNumerator = 0;
        uint32_t AspectRatioDenominator = 0;

        uint32_t OpenGLMajorVersion = 4;
        uint32_t OpenGLMinorVersion = 1;
        bool OpenGLDebugContext = false;
    };

    enum class Cursor;

    class Window
    {
    public:
        virtual ~Window() = default;

        virtual void OnUpdate() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetFramebufferWidth() const = 0;
        virtual uint32_t GetFramebufferHeight() const = 0;
        virtual glm::ivec2 GetPosition() const = 0;
        virtual glm::vec2 GetContentScale() const = 0;
        virtual WindowMode GetMode() const = 0;
        virtual bool IsHidden() const = 0;
        virtual bool IsFocused() const = 0;
        virtual bool IsMinimized() const = 0;
        virtual bool IsMaximized() const = 0;
        virtual bool IsCursorGrabbed() const = 0;
        virtual bool ShouldClose() const = 0;

        virtual void SetTitle(const String& title) = 0;
        virtual const String& GetTitle() const = 0;

        virtual void SetCursor(Cursor cursor) = 0;
        virtual Cursor GetCursor() const = 0;
        virtual void SetCursorGrabbed(bool grabbed) = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void* GetNativeWindow() const = 0;

        virtual void SetHidden(bool hidden) = 0;
        virtual void Move(int32_t left, int top) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;
        virtual void SetFullscreen(uint32_t width, uint32_t height, uint32_t refreshRate, uint32_t monitorIdx) = 0;
        virtual void SetBorderlessFullscreen(uint32_t monitorIdx) = 0;
        virtual void SetWindowed(uint32_t width, uint32_t height) = 0;
        virtual void SetSizeLimits(uint32_t minWidth, uint32_t minHeight, uint32_t maxWidth = 0, uint32_t maxHeight = 0) = 0;
        virtual void SetAspectRatio(uint32_t numerator, uint32_t denominator) = 0;
        virtual void RequestClose() = 0;

        glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPosition) const
        {
            const glm::ivec2 position = GetPosition();
            return screenPosition - glm::vec2(position);
        }

        glm::vec2 WindowToScreenPosition(const glm::vec2& windowPosition) const { return windowPosition + glm::vec2(GetPosition()); }

    public:
        static bool Initialize();
        static void Shutdown();
        static bool IsInitialized();
        static void PollEvents();
        static Scope<Window> Create(const WindowDesc& windowDesc);

    protected:
        static bool RegisterNativeWindow();
        static void UnregisterNativeWindow();
    };
} // namespace Crowny
