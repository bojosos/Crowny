#pragma once

#include "Crowny/Events/Event.h"
#include "Crowny/Input/Input.h"

namespace Crowny
{
    class KeyEvent : public Event
    {
    public:
        KeyCode GetKeyCode() const { return m_KeyCode; }
        int32_t GetScanCode() const { return m_ScanCode; }
        InputModifiers GetModifiers() const { return m_Modifiers; }
        double GetTimestamp() const { return m_Timestamp; }
        uint32_t GetDeviceId() const { return m_DeviceId; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    protected:
        KeyEvent(const KeyCode keycode, int32_t scanCode, InputModifiers modifiers, double timestamp, uint32_t deviceId)
          : m_KeyCode(keycode), m_ScanCode(scanCode), m_Modifiers(modifiers), m_Timestamp(timestamp), m_DeviceId(deviceId)
        {
        }

        KeyCode m_KeyCode;
        int32_t m_ScanCode;
        InputModifiers m_Modifiers;
        double m_Timestamp;
        uint32_t m_DeviceId;
    };

    class KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(const KeyCode keycode, const uint16_t repeatCount, int32_t scanCode = 0, InputModifiers modifiers = {},
                        double timestamp = 0.0, uint32_t deviceId = 0)
          : KeyEvent(keycode, scanCode, modifiers, timestamp, deviceId), m_RepeatCount(repeatCount)
        {
        }

        uint16_t GetRepeatCount() const { return m_RepeatCount; }
        bool IsRepeat() const { return m_RepeatCount > 0; }

        String ToString() const override
        {
            StringStream ss;
            ss << "KeyPressedEvent: " << m_KeyCode << "(" << m_RepeatCount << "repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)

    private:
        uint16_t m_RepeatCount;
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(const KeyCode keycode, int32_t scanCode = 0, InputModifiers modifiers = {}, double timestamp = 0.0, uint32_t deviceId = 0)
          : KeyEvent(keycode, scanCode, modifiers, timestamp, deviceId)
        {
        }

        String ToString() const override
        {
            StringStream ss;
            ss << "KeyReleasedEvent: " << m_KeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class KeyTypedEvent : public Event
    {
    public:
        explicit KeyTypedEvent(uint32_t codepoint, double timestamp = 0.0, uint32_t deviceId = 0)
          : m_Codepoint(codepoint), m_Timestamp(timestamp), m_DeviceId(deviceId)
        {
        }

        uint32_t GetCodepoint() const { return m_Codepoint; }
        double GetTimestamp() const { return m_Timestamp; }
        uint32_t GetDeviceId() const { return m_DeviceId; }

        String ToString() const override
        {
            StringStream ss;
            ss << "KeyTypedEvent: " << m_Codepoint;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyTyped)
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    private:
        uint32_t m_Codepoint;
        double m_Timestamp;
        uint32_t m_DeviceId;
    };
} // namespace Crowny
