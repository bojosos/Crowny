#pragma once

#include "Crowny/Events/Event.h"

namespace Crowny
{
    struct WindowStateSnapshot
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t FramebufferWidth = 0;
        uint32_t FramebufferHeight = 0;
        int32_t Left = 0;
        int32_t Top = 0;
        float ContentScaleX = 1.0f;
        float ContentScaleY = 1.0f;
        bool Focused = false;
        bool Minimized = false;
        bool Maximized = false;

        bool operator==(const WindowStateSnapshot&) const = default;
    };

    /** Collects platform window state and publishes one coherent event batch. */
    class WindowEventState
    {
    public:
        WindowEventState() = default;
        explicit WindowEventState(const WindowStateSnapshot& initialState);

        void Reset(const WindowStateSnapshot& state);
        void SetSnapshot(const WindowStateSnapshot& state);
        void SetWindowSize(int32_t width, int32_t height);
        void SetFramebufferSize(int32_t width, int32_t height);
        void SetPosition(int32_t left, int32_t top);
        void SetContentScale(float xScale, float yScale);
        void SetFocused(bool focused);
        void SetMinimized(bool minimized);
        void SetMaximized(bool maximized);

        bool HasPendingChanges() const { return !(m_Current == m_Published); }
        const WindowStateSnapshot& GetCurrent() const { return m_Current; }

        void Flush(const EventCallbackFn& callback);

    private:
        static uint32_t ClampDimension(int32_t value);

        WindowStateSnapshot m_Current;
        WindowStateSnapshot m_Published;
    };
} // namespace Crowny
