#pragma once

#include "Crowny/Window/RenderWindow.h"

#include "Platform/OpenGL/OpenGLContext.h"

namespace Crowny
{
    class OpenGLRenderWindow : public RenderWindow
    {
    public:
        friend class RenderWindow;
        ~OpenGLRenderWindow() override = default;

        void SwapBuffers(uint32_t syncMask = 0xFFFFFFFF) override;
        glm::vec2 ScreenToWindowPosition(const glm::vec2& screenPos) override;
        glm::vec2 WindowToScreenPosition(const glm::vec2& windowPos) override;
        void Resize(uint32_t width, uint32_t height) override;
        void Move(int32_t left, int32_t top) override;
        void Minimize() override;
        void Maximize() override;
        void Restore() override;
        void SetFullscreen(uint32_t width, uint32_t height, float refreshRate = 60.0f, uint32_t monitorIdx = 0) override;
        void SetBorderlessFullscreen(uint32_t monitorIdx = 0) override;
        void SetWindowed(uint32_t width, uint32_t height) override;
        Window* GetWindow() const override { return m_Window.get(); }
        void SetHidden(bool hidden) override;
        void SetVSync(bool enabled) override;
        const RenderTargetProperties& GetProperties() const override;

        OpenGLContext* GetContext() const { return m_Context.get(); }

    protected:
        explicit OpenGLRenderWindow(const RenderWindowDesc& renderWindowDesc);

    private:
        void SyncWindowProperties() const;

        Scope<Window> m_Window;
        Scope<OpenGLContext> m_Context;
        bool m_ShowOnSwap = false;
        mutable RenderWindowProperties m_Properties;
    };
} // namespace Crowny
