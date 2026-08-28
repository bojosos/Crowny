#pragma once

#include "Crowny/Events/Event.h"
#include "Crowny/Input/Input.h"

namespace Crowny
{

    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y, const glm::vec2& delta = glm::vec2(0.0f), double timestamp = 0.0, uint32_t deviceId = 0)
          : m_MouseX(x), m_MouseY(y), m_Delta(delta), m_Timestamp(timestamp), m_DeviceId(deviceId)
        {
        }

        float GetX() const { return m_MouseX; }
        float GetY() const { return m_MouseY; }
        glm::vec2 GetDelta() const { return m_Delta; }
        float GetDeltaX() const { return m_Delta.x; }
        float GetDeltaY() const { return m_Delta.y; }
        double GetTimestamp() const { return m_Timestamp; }
        uint32_t GetDeviceId() const { return m_DeviceId; }

        String ToString() const override
        {
            StringStream ss;
            ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float m_MouseX, m_MouseY;
        glm::vec2 m_Delta;
        double m_Timestamp;
        uint32_t m_DeviceId;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const float xOffset, const float yOffset, double timestamp = 0.0, uint32_t deviceId = 0)
          : m_XOffset(xOffset), m_YOffset(yOffset), m_Timestamp(timestamp), m_DeviceId(deviceId)
        {
        }

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }
        double GetTimestamp() const { return m_Timestamp; }
        uint32_t GetDeviceId() const { return m_DeviceId; }

        String ToString() const override
        {
            StringStream ss;
            ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    private:
        float m_XOffset, m_YOffset;
        double m_Timestamp;
        uint32_t m_DeviceId;
    };

    class MouseButtonEvent : public Event
    {
    public:
        MouseCode GetMouseButton() const { return m_Button; }
        glm::vec2 GetPosition() const { return m_Position; }
        InputModifiers GetModifiers() const { return m_Modifiers; }
        double GetTimestamp() const { return m_Timestamp; }
        uint32_t GetDeviceId() const { return m_DeviceId; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    protected:
        MouseButtonEvent(const MouseCode button, const glm::vec2& position = glm::vec2(0.0f), InputModifiers modifiers = {}, double timestamp = 0.0,
                         uint32_t deviceId = 0)
          : m_Button(button), m_Position(position), m_Modifiers(modifiers), m_Timestamp(timestamp), m_DeviceId(deviceId)
        {
        }

        MouseCode m_Button;
        glm::vec2 m_Position;
        InputModifiers m_Modifiers;
        double m_Timestamp;
        uint32_t m_DeviceId;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(const MouseCode button, const glm::vec2& position = glm::vec2(0.0f), InputModifiers modifiers = {},
                                double timestamp = 0.0, uint32_t deviceId = 0)
          : MouseButtonEvent(button, position, modifiers, timestamp, deviceId)
        {
        }

        String ToString() const override
        {
            StringStream ss;
            ss << "MouseButtonPressedEvent: " << m_Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(const MouseCode button, const glm::vec2& position = glm::vec2(0.0f), InputModifiers modifiers = {},
                                 double timestamp = 0.0, uint32_t deviceId = 0)
          : MouseButtonEvent(button, position, modifiers, timestamp, deviceId)
        {
        }

        String ToString() const override
        {
            StringStream ss;
            ss << "MouseButtonReleasedEvent: " << m_Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };
} // namespace Crowny
