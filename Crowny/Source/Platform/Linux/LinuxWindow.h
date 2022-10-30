#pragma once
// #if 0

#include "Crowny/RenderAPI/GraphicsContext.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>

namespace Crowny
{
    class WindowDescription;

    class LinuxWindow : public Window
    {
    public:
        LinuxWindow(const WindowDesc& windowDesc);
        ~LinuxWindow();

        virtual void OnUpdate() override;

        virtual uint32_t GetWidth() const override { return m_Data.Width; }
        virtual uint32_t GetHeight() const override { return m_Data.Height; }
        virtual bool GetVSync() const override { return m_Data.VSync; };

        virtual const String& GetTitle() const override { return m_Data.Title; }
        virtual void SetTitle(const String& title) override;

        virtual void SetCursor(Cursor cursor) override;

        virtual void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; };
        virtual void* GetNativeWindow() const override { return m_Window; }

        virtual void Move(int32_t left, int32_t top) override;
        virtual void Resize(uint32_t width, uint32_t height) override;
        virtual void SetHidden(bool hidden) override;
		virtual void Minimize() override;
		virtual void Maximize() override;
		virtual void Restore() override;

    private:
        void Init(const WindowDesc& windowDesc);
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
  // #endif