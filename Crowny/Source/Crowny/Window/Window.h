#pragma once

#include "Crowny/Events/Event.h"

namespace Crowny
{

    struct WindowDesc
    {
        bool ShowTitleBar = true;
        bool ShowBorder = true;
        bool AllowResize = true;
        bool Fullscreen = false;
        uint32_t Width = 0;
        uint32_t Height = 0;
        bool Hidden = false;
        uint32_t Left = -1;
        uint32_t Top = -1;
        String Title;
        bool Modal = false;
        bool StartMaximized = false;
        uint32_t MonitorIdx = 0;
    };

    enum class Cursor;

    class Window
    {
    public:
        virtual ~Window() = default;

        virtual void OnUpdate() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual bool GetVSync() const = 0;

        virtual void SetTitle(const String& title) = 0;
        virtual const String& GetTitle() const = 0;

        virtual void SetCursor(Cursor cursor) = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void* GetNativeWindow() const = 0;

        virtual void SetHidden(bool hidden) = 0;
        virtual void Move(int32_t left, int top) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void Restore() = 0;

    public:
        static Window* Create(const WindowDesc& windowDesc);
    };
} // namespace Crowny