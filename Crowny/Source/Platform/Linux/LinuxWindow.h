#pragma once

#include "Crowny/RenderAPI/GraphicsContext.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>

namespace Crowny
{

    class LinuxWindow : public Window
    {
    public:
        LinuxWindow(const WindowProperties& props);
        ~LinuxWindow();

        virtual void OnUpdate() override;

        virtual uint32_t GetWidth() const override { return m_Data.Width; }
        virtual uint32_t GetHeight() const override { return m_Data.Height; }
        virtual bool GetVSync() const override { return m_Data.VSync; };
        virtual const std::string& GetTitle() const override { return m_Data.Title; }

        virtual void SetCursor(Cursor cursor) override;

        virtual void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; };
        virtual void* GetNativeWindow() const override { return m_Window; }

        virtual void SetVSync(bool enabled) override;
        virtual bool IsVSync() const override;

    private:
        void Init(const WindowProperties& props);
        void Shutdown();

    private:
        GLFWwindow* m_Window;
        GLFWcursor* m_Cursor = nullptr;
        Scope<GraphicsContext> m_Context;

        struct WindowData
        {
            std::string Title;
            uint32_t Width, Height;
            bool VSync;
            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
    };
} // namespace Crowny
