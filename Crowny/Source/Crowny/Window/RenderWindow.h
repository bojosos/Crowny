#pragma once

#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/Window/Window.h"

namespace Crowny
{

    struct RenderWindowProperties : public RenderTargetProperties
    {
        RenderWindowProperties(const RenderWindowDesc& renderWindowDesc);
        virtual ~RenderWindowProperties() = default;

        WindowMode Mode = WindowMode::Windowed;
        bool Fullscreen = false;
        int32_t Left = 0;
        int32_t Top = 0;
        bool IsFocused = false;
        bool IsHidden = false;
        bool IsMinimized = false;
        bool IsModal = false;
        bool IsMaximized = false;
        bool VSync = true;
    };

    struct RenderWindowDesc
    {
        String Title = "Crowny Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        WindowMode Mode = WindowMode::Windowed;
        // Kept for source compatibility. Mode takes precedence when it is not Windowed.
        bool Fullscreen = false;
        bool StartMaximized = false;
        bool VSync = true;
        bool AllowResize = true;
        bool ShowTitleBar = true;
        bool Hidden = false;
        uint32_t MonitorIdx = 0;

        uint32_t VsyncInterval = 1;
        bool DepthBuffer = true;
        uint32_t Samples = 0;
        bool ShowBorder = true;
        bool HideUntilSwap = false;
        int32_t Left = -1;
        int32_t Top = -1;
        bool Modal = false;
        uint32_t MinWidth = 1;
        uint32_t MinHeight = 1;
        uint32_t MaxWidth = 0;
        uint32_t MaxHeight = 0;
        uint32_t AspectRatioNumerator = 0;
        uint32_t AspectRatioDenominator = 0;
    };

    class RenderWindow : public RenderTarget
    {
    public:
        virtual ~RenderWindow() = default;

        //   virtual void SwapBuffers(uint32_t syncMask) = 0;
        virtual glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) = 0;
        virtual glm::vec2 WindowToScreenPosition(const glm::vec2& windowPos) = 0;
        glm::vec2 WindowToScreenPos(const glm::vec2& windowPos) { return WindowToScreenPosition(windowPos); }
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Move(int32_t left, int32_t top) = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;
        virtual void SetFullscreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0) = 0;
        virtual void SetBorderlessFullscreen(uint32_t monitorIdx = 0) = 0;
        virtual void SetWindowed(uint32_t width, uint32_t height) = 0;
        void SetFullScreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0)
        {
            SetFullscreen(width, height, refreshRate, monitorIdx);
        }
        virtual Window* GetWindow() const = 0;

        virtual void SetHidden(bool hidden) = 0;
        virtual void SetVSync(bool enabled) = 0;

        virtual const RenderTargetProperties& GetProperties() const = 0;

    public:
        static Ref<RenderWindow> Create(const RenderWindowDesc& renderWindowDesc);

    protected:
        RenderWindow(const RenderWindowDesc& renderWindowDesc);

        RenderWindowDesc m_Desc;
    };
} // namespace Crowny
