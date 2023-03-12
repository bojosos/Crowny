#pragma once

#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/Window/Window.h"

namespace Crowny
{

    struct RenderWindowProperties : public RenderTargetProperties
    {
        RenderWindowProperties(const RenderWindowDesc& renderWindowDesc);
        virtual ~RenderWindowProperties() = default;

        bool Fullscreen = false; // not supported
        int32_t Left = 0;
        int32_t Top = 0;
        bool IsFocused = false;
        bool IsHidden = false;
        bool IsModal = false;
        bool IsMaximized = false;
        bool VSync = true;
    };

    struct RenderWindowDesc
    {
        String Title = "Crowny Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool Fullscreen = false;
        bool StartMaximized = false;
        bool VSync = true;
        bool AllowResize = true;
        bool ShowTitleBar = true;
        bool Hidden = false;
        uint32_t MonitorIdx = 0;

        uint32_t VsyncInterval = 1; // not supported
        bool DepthBuffer = true;    // not supported
        uint32_t Samples = 0;       // not supported
        bool ShowBorder = true;     // not supported
        bool HideUntilSwap = false;
        int32_t Left = -1;
        int32_t Top = -1;
        bool Modal = false;
    };

    class RenderWindow : public RenderTarget
    {
    public:
        virtual ~RenderWindow() = default;

        //   virtual void SwapBuffers(uint32_t syncMask) = 0;
        virtual glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) = 0;
        virtual glm::vec2 WindowToScreenPos(const glm::vec2& windowPos) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Move(int32_t left, int32_t top) = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;
        virtual void SetFullScreen(uint32_t width, uint32_t height, float refreshRate = 60.0f,
                                   uint32_t monitorIdx = 0) = 0;
        virtual void SetWindowed(uint32_t width, uint32_t height) = 0;
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
