#include "cwtpch.h"

#include "Crowny/Input/InputMap.h"

using namespace Crowny;

namespace
{
    class FakeInputStateReader final : public InputStateReader
    {
    public:
        DigitalInputState ReadKey(KeyCode key) const override { return ReadDigital(Keys, key); }
        DigitalInputState ReadMouseButton(MouseCode button) const override { return ReadDigital(MouseButtons, button); }
        bool IsGamepadConnected(uint32_t gamepadIndex) const override { return ConnectedGamepads.contains(gamepadIndex); }

        DigitalInputState ReadGamepadButton(uint32_t gamepadIndex, GamepadButtonCode button) const override
        {
            return ReadDigital(GamepadButtons, GamepadControl(gamepadIndex, button));
        }

        float ReadGamepadAxis(uint32_t gamepadIndex, GamepadAxisCode axis) const override
        {
            const auto value = GamepadAxes.find(GamepadControl(gamepadIndex, axis));
            return value == GamepadAxes.end() ? 0.0f : value->second;
        }

        UnorderedMap<uint32_t, DigitalInputState> Keys;
        UnorderedMap<uint32_t, DigitalInputState> MouseButtons;
        UnorderedSet<uint32_t> ConnectedGamepads;
        UnorderedMap<uint64_t, DigitalInputState> GamepadButtons;
        UnorderedMap<uint64_t, float> GamepadAxes;

    private:
        static uint64_t GamepadControl(uint32_t gamepadIndex, uint32_t code)
        {
            return static_cast<uint64_t>(code) | (static_cast<uint64_t>(gamepadIndex) << 32);
        }

        template <typename Map, typename Key> static DigitalInputState ReadDigital(const Map& states, Key key)
        {
            const auto state = states.find(key);
            return state == states.end() ? DigitalInputState{} : state->second;
        }
    };

    InputBinding KeyBinding(KeyCode key, InputBindingPart part = InputBindingPart::Whole)
    {
        InputBinding binding;
        binding.Device = InputBindingDevice::Keyboard;
        binding.Code = key;
        binding.Part = part;
        return binding;
    }

    InputAction Action(String name, InputActionType type, std::initializer_list<InputBinding> bindings)
    {
        InputAction action;
        action.Name = std::move(name);
        action.Type = type;
        action.Bindings = bindings;
        return action;
    }

    InputContext Context(String name, int32_t priority, std::initializer_list<InputAction> actions, bool consumeInput = true)
    {
        InputContext context;
        context.Name = std::move(name);
        context.Priority = priority;
        context.ConsumeInput = consumeInput;
        context.Actions = actions;
        return context;
    }
} // namespace

TEST_CASE("Input maps evaluate modifier chords and frame transitions", "[Input]")
{
    InputBinding binding = KeyBinding(Key::K);
    binding.Modifiers = InputModifierBits::Control;
    InputMap map({ Context("Gameplay", 0, { Action("Quick save", InputActionType::Button, { binding }) }) });
    FakeInputStateReader input;
    input.Keys[Key::LeftControl] = { true, true, false };
    input.Keys[Key::K] = { true, true, false };

    map.Update(input);
    CHECK(map.IsPressed("Quick save"));
    CHECK(map.IsHeld("Quick save"));

    input.Keys[Key::LeftControl] = { true, false, false };
    input.Keys[Key::K] = { true, false, false };
    map.Update(input);
    CHECK_FALSE(map.IsPressed("Quick save"));
    CHECK(map.IsHeld("Quick save"));

    input.Keys[Key::K] = { false, false, true };
    map.Update(input);
    CHECK(map.IsReleased("Quick save"));
    CHECK_FALSE(map.IsHeld("Quick save"));
}

TEST_CASE("Input maps combine a normalized WASD composite", "[Input]")
{
    InputMap map({ Context("Gameplay", 0,
                           { Action("Move", InputActionType::Axis2D,
                                    { KeyBinding(Key::W, InputBindingPart::Up), KeyBinding(Key::S, InputBindingPart::Down),
                                      KeyBinding(Key::A, InputBindingPart::Left), KeyBinding(Key::D, InputBindingPart::Right) }) }) });
    FakeInputStateReader input;
    input.Keys[Key::W].Held = true;
    input.Keys[Key::D].Held = true;

    map.Update(input);
    const glm::vec2 move = map.GetAxis2D("Move");
    CHECK(move.x == Catch::Approx(0.7071067f));
    CHECK(move.y == Catch::Approx(0.7071067f));
}

TEST_CASE("Input maps apply gamepad dead zones, scale, and inversion", "[Input]")
{
    InputBinding lookX;
    lookX.Device = InputBindingDevice::GamepadAxis;
    lookX.Code = GamepadAxis::RightX;
    lookX.Part = InputBindingPart::X;
    lookX.DeadZone = 0.2f;
    lookX.Scale = 0.5f;
    lookX.Invert = true;

    InputMap map({ Context("Gameplay", 0, { Action("Look", InputActionType::Axis2D, { lookX }) }) });
    FakeInputStateReader input;
    input.ConnectedGamepads.insert(0);
    input.GamepadAxes[GamepadAxis::RightX] = 0.1f;
    map.Update(input);
    CHECK(map.GetAxis2D("Look").x == Catch::Approx(0.0f));

    input.GamepadAxes[GamepadAxis::RightX] = 0.6f;
    map.Update(input);
    CHECK(map.GetAxis2D("Look").x == Catch::Approx(-0.25f));
}

TEST_CASE("Higher priority contexts override names and consume active controls", "[Input]")
{
    InputMap map({ Context("Gameplay", 0,
                           { Action("Confirm", InputActionType::Button, { KeyBinding(Key::Enter) }),
                             Action("Interact", InputActionType::Button, { KeyBinding(Key::E) }) }),
                   Context("Menu", 100,
                           { Action("Confirm", InputActionType::Button, { KeyBinding(Key::Space) }),
                             Action("Menu item", InputActionType::Button, { KeyBinding(Key::E) }) }) });
    FakeInputStateReader input;
    input.Keys[Key::Enter].Held = true;
    input.Keys[Key::E].Held = true;

    map.Update(input);
    CHECK_FALSE(map.IsHeld("Confirm"));
    CHECK(map.IsHeld("Menu item"));
    CHECK_FALSE(map.IsHeld("Interact"));

    REQUIRE(map.SetContextEnabled("Menu", false));
    map.Update(input);
    CHECK(map.IsHeld("Confirm"));
    CHECK(map.IsHeld("Interact"));
}

TEST_CASE("Runtime rebinds do not change authored defaults", "[Input]")
{
    InputMap map({ Context("Gameplay", 0, { Action("Jump", InputActionType::Button, { KeyBinding(Key::Space) }) }) });
    map.EnsureStableIds();
    const UUID bindingId = map.GetContexts()[0].Actions[0].Bindings[0].Id;

    InputBinding rebound;
    rebound.Device = InputBindingDevice::Mouse;
    rebound.Code = Mouse::ButtonRight;
    REQUIRE(map.Rebind(bindingId, rebound));

    FakeInputStateReader input;
    input.MouseButtons[Mouse::ButtonRight] = { true, true, false };
    map.Update(input);
    CHECK(map.IsHeld("Jump"));
    CHECK(map.GetContexts()[0].Actions[0].Bindings[0].Device == InputBindingDevice::Keyboard);

    REQUIRE(map.ClearRebind(bindingId));
    map.Update(input);
    CHECK_FALSE(map.IsHeld("Jump"));
}

TEST_CASE("Input capture filters keyboard and mouse actions only", "[Input]")
{
    InputBinding mouse;
    mouse.Device = InputBindingDevice::Mouse;
    mouse.Code = Mouse::ButtonLeft;
    InputBinding gamepad;
    gamepad.Device = InputBindingDevice::GamepadButton;
    gamepad.Code = GamepadButton::A;

    InputMap map({ Context("Gameplay", 0,
                           { Action("Keyboard", InputActionType::Button, { KeyBinding(Key::Space) }),
                             Action("Pointer", InputActionType::Button, { mouse }), Action("Controller", InputActionType::Button, { gamepad }) },
                           false) });
    FakeInputStateReader input;
    input.Keys[Key::Space].Held = true;
    input.MouseButtons[Mouse::ButtonLeft].Held = true;
    input.ConnectedGamepads.insert(0);
    input.GamepadButtons[GamepadButton::A].Held = true;
    map.SetCapture(true, true);

    map.Update(input);
    CHECK_FALSE(map.IsHeld("Keyboard"));
    CHECK_FALSE(map.IsHeld("Pointer"));
    CHECK(map.IsHeld("Controller"));
}
