#pragma once

#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>
#include <array>

namespace Crowny
{

    // GLFW owns the native implementation on every desktop platform. The file name is
    // retained so existing generated projects do not need to be regenerated.
    class LinuxWindow final : public Window
    {
    public:
        explicit LinuxWindow(const WindowDesc& windowDesc);
        ~LinuxWindow() override;

        void OnUpdate() override {}

        uint32_t GetWidth() const override { return m_Data.Width; }
        uint32_t GetHeight() const override { return m_Data.Height; }
        uint32_t GetFramebufferWidth() const override { return m_Data.FramebufferWidth; }
        uint32_t GetFramebufferHeight() const override { return m_Data.FramebufferHeight; }
        glm::ivec2 GetPosition() const override { return { m_Data.Left, m_Data.Top }; }
        glm::vec2 GetContentScale() const override { return { m_Data.ContentScaleX, m_Data.ContentScaleY }; }
        WindowMode GetMode() const override { return m_Mode; }
        bool IsHidden() const override;
        bool IsFocused() const override;
        bool IsMinimized() const override;
        bool IsMaximized() const override;
        bool IsCursorGrabbed() const override { return m_CursorGrabbed || m_CursorType == Cursor::NO_CURSOR; }
        bool ShouldClose() const override;

        const String& GetTitle() const override { return m_Data.Title; }
        void SetTitle(const String& title) override;
        void SetCursor(Cursor cursor) override;
        void SetCursorGrabbed(bool grabbed) override;

        void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void* GetNativeWindow() const override { return m_Window; }

        void Move(int32_t left, int32_t top) override;
        void Resize(uint32_t width, uint32_t height) override;
        void SetHidden(bool hidden) override;
        void Minimize() override;
        void Maximize() override;
        void Restore() override;
        void SetFullscreen(uint32_t width, uint32_t height, uint32_t refreshRate, uint32_t monitorIdx) override;
        void SetBorderlessFullscreen(uint32_t monitorIdx) override;
        void SetWindowed(uint32_t width, uint32_t height) override;
        void SetSizeLimits(uint32_t minWidth, uint32_t minHeight, uint32_t maxWidth = 0, uint32_t maxHeight = 0) override;
        void SetAspectRatio(uint32_t numerator, uint32_t denominator) override;
        void RequestClose() override;

    private:
        struct WindowData
        {
            String Title;
            uint32_t Width = 0;
            uint32_t Height = 0;
            uint32_t FramebufferWidth = 0;
            uint32_t FramebufferHeight = 0;
            uint32_t LastEventWidth = 0;
            uint32_t LastEventHeight = 0;
            uint32_t LastEventFramebufferWidth = 0;
            uint32_t LastEventFramebufferHeight = 0;
            int32_t Left = 0;
            int32_t Top = 0;
            float ContentScaleX = 1.0f;
            float ContentScaleY = 1.0f;
            EventCallbackFn EventCallback;
        };

        void Init(const WindowDesc& windowDesc);
        void Shutdown();
        void InstallCallbacks();
        void Dispatch(Event& event);
        void UpdateDimensions();
        void DispatchResizeIfChanged();
        void RememberWindowedRect();
        void ApplyCursor();
        GLFWcursor* GetOrCreateCursor(Cursor cursor);
        GLFWmonitor* GetMonitor(uint32_t monitorIdx) const;

        GLFWwindow* m_Window = nullptr;
        std::array<GLFWcursor*, 7> m_Cursors{};
        WindowDesc m_Desc;
        WindowData m_Data;
        WindowMode m_Mode = WindowMode::Windowed;
        Cursor m_CursorType = Cursor::POINTER;
        bool m_CursorGrabbed = false;
        int32_t m_WindowedLeft = 0;
        int32_t m_WindowedTop = 0;
        uint32_t m_WindowedWidth = 1280;
        uint32_t m_WindowedHeight = 720;
    };

} // namespace Crowny
