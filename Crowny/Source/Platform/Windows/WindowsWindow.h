#pragma once

#include "Crowny/RenderAPI/GraphicsContext.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>

namespace Crowny
{

    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProperties& props);
        ~WindowsWindow();

        virtual void OnUpdate() override;

        virtual uint32_t GetWidth() const override { return m_Data.Width; }
        virtual uint32_t GetHeight() const override { return m_Data.Height; }
        virtual bool GetVSync() const override { return m_Data.VSync; };

        virtual void SetCursor(Cursor cursor) override;

        virtual void SetTitle(const String& title) override;
        virtual const String& GetTitle() const override { return m_Data.Title; }

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
            String Title;
            uint32_t Width, Height;
            bool VSync;
            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
    };
} // namespace Crowny
