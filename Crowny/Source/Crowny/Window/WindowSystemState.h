#pragma once

#include <cstdint>

namespace Crowny::Detail
{
    enum class WindowSystemAction
    {
        None,
        TerminateBackend
    };

    /** Tracks native windows so the shared window backend outlives every handle it created. */
    class WindowSystemState
    {
    public:
        bool IsInitialized() const { return m_Initialized; }
        bool IsShutdownPending() const { return m_ShutdownPending; }
        uint32_t GetLiveWindowCount() const { return m_LiveWindowCount; }

        void MarkInitialized()
        {
            m_Initialized = true;
            m_ShutdownPending = false;
        }

        void MarkInitializationFailed()
        {
            m_Initialized = false;
            m_ShutdownPending = false;
            m_LiveWindowCount = 0;
        }

        void CancelPendingShutdown()
        {
            if (m_Initialized)
                m_ShutdownPending = false;
        }

        bool RegisterWindow()
        {
            if (!m_Initialized)
                return false;
            ++m_LiveWindowCount;
            return true;
        }

        WindowSystemAction UnregisterWindow()
        {
            if (m_LiveWindowCount == 0)
                return WindowSystemAction::None;

            --m_LiveWindowCount;
            if (m_LiveWindowCount == 0 && m_ShutdownPending)
                return MarkTerminated();
            return WindowSystemAction::None;
        }

        WindowSystemAction RequestShutdown()
        {
            if (!m_Initialized)
                return WindowSystemAction::None;
            if (m_LiveWindowCount != 0)
            {
                m_ShutdownPending = true;
                return WindowSystemAction::None;
            }
            return MarkTerminated();
        }

    private:
        WindowSystemAction MarkTerminated()
        {
            m_Initialized = false;
            m_ShutdownPending = false;
            return WindowSystemAction::TerminateBackend;
        }

        uint32_t m_LiveWindowCount = 0;
        bool m_Initialized = false;
        bool m_ShutdownPending = false;
    };
} // namespace Crowny::Detail
