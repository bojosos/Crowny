#pragma once

#include "Crowny/Window/Window.h"
#include "Crowny/Renderer/RenderTarget.h"

namespace Crowny
{
    
    struct RenderWindowProperties : public RenderTargetProperties
    {
        bool Fullscreen = false; // not supported
        std::string Title = "";
        bool Vsync = true;
        uint32_t VsyncInterval = 1; // not supported
        bool Hidden = false; // not supported
        bool DepthBuffer = true; // not supported
        uint32_t Samples = 0; // not supported
        bool ShowTitleBar = true; // not supported
        bool ShowBorder = true; // not supported
        bool AllowResize = true; // not supported
        bool HideUntilSwap = false;  // not supported
    };
    
    class RenderWindow : public RenderTarget
    {
    public:
        virtual ~RenderWindow() = default;
        
     //   virtual void SwapBuffers(uint32_t syncMask) = 0;
        virtual glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) = 0;
        virtual glm::vec2 WindowToScreenPos(const glm::vec2& windowPos) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Hide() = 0;
        virtual void Move(float left, float top) = 0;
        virtual void Show() = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;
        virtual void SetFullScreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0) = 0;
        virtual void SetWindowed(uint32_t width, uint32_t height) = 0;
        virtual Window* GetWindow() const = 0;

        const RenderWindowProperties& GetProperties() const { return m_Properties; };
        static Ref<RenderWindow> Create(const RenderWindowProperties& props);
        
    protected:
        RenderWindow(const RenderWindowProperties& props);
        RenderWindowProperties m_Properties;
        uint32_t m_WindowId;
    };
}
