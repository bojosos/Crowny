#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Input/Input.h"

namespace Crowny
{
    enum class InputActionType : uint8_t
    {
        Button,
        Axis1D,
        Axis2D
    };

    enum class InputBindingDevice : uint8_t
    {
        Keyboard,
        Mouse,
        GamepadButton,
        GamepadAxis
    };

    enum class InputBindingPart : uint8_t
    {
        Whole,
        Positive,
        Negative,
        Up,
        Down,
        Left,
        Right,
        X,
        Y
    };

    struct InputBinding
    {
        UUID Id = UUID::EMPTY;
        InputBindingDevice Device = InputBindingDevice::Keyboard;
        InputBindingPart Part = InputBindingPart::Whole;
        uint32_t Code = Key::Space;
        uint32_t GamepadIndex = 0;
        InputModifiers Modifiers;
        float Scale = 1.0f;
        float DeadZone = 0.15f;
        bool Invert = false;
    };

    struct InputAction
    {
        UUID Id = UUID::EMPTY;
        String Name = "Action";
        InputActionType Type = InputActionType::Button;
        float PressThreshold = 0.5f;
        Vector<InputBinding> Bindings;
    };

    struct InputContext
    {
        UUID Id = UUID::EMPTY;
        String Name = "Context";
        int32_t Priority = 0;
        bool Enabled = true;
        bool ConsumeInput = true;
        Vector<InputAction> Actions;
    };

    struct DigitalInputState
    {
        bool Held = false;
        bool Pressed = false;
        bool Released = false;
    };

    struct InputActionValue
    {
        InputActionType Type = InputActionType::Button;
        glm::vec2 Value = glm::vec2(0.0f);

        bool AsButton(float threshold = 0.5f) const;
        float AsAxis1D() const { return Value.x; }
        glm::vec2 AsAxis2D() const { return Value; }
    };

    /**
     * Supplies device state to InputMap. The default adapter reads Crowny::Input;
     * tests and tools can supply a deterministic reader without a platform window.
     */
    class InputStateReader
    {
    public:
        virtual ~InputStateReader() = default;

        virtual DigitalInputState ReadKey(KeyCode key) const = 0;
        virtual DigitalInputState ReadMouseButton(MouseCode button) const = 0;
        virtual bool IsGamepadConnected(uint32_t gamepadIndex) const = 0;
        virtual DigitalInputState ReadGamepadButton(uint32_t gamepadIndex, GamepadButtonCode button) const = 0;
        virtual float ReadGamepadAxis(uint32_t gamepadIndex, GamepadAxisCode axis) const = 0;
    };
} // namespace Crowny
