#include <catch2/catch_test_macros.hpp>

#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Input/Input.h"

using namespace Crowny;

TEST_CASE("Input transitions remain latched for the frame", "[Input]")
{
    Detail::InputButtonState state;

    state.SetHeld(true);
    state.SetHeld(false);

    CHECK_FALSE(state.IsHeld());
    CHECK(state.WasPressed());
    CHECK(state.WasReleased());

    state.BeginFrame();
    CHECK_FALSE(state.WasPressed());
    CHECK_FALSE(state.WasReleased());
}

TEST_CASE("Input transition state ignores repeated held values", "[Input]")
{
    Detail::InputButtonState state;

    state.SetHeld(true);
    state.SetHeld(true);
    CHECK(state.IsHeld());
    CHECK(state.WasPressed());
    CHECK_FALSE(state.WasReleased());

    state.BeginFrame();
    state.SetHeld(true);
    CHECK_FALSE(state.WasPressed());
    CHECK_FALSE(state.WasReleased());
}

TEST_CASE("Input events retain native metadata", "[Input][Events]")
{
    InputModifiers modifiers;
    modifiers.Set(InputModifierBits::Control).Set(InputModifierBits::Shift);

    KeyPressedEvent key(Key::A, 1, 30, modifiers, 12.5, 3);
    CHECK(key.GetKeyCode() == Key::A);
    CHECK(key.GetScanCode() == 30);
    CHECK(key.IsRepeat());
    CHECK(key.GetTimestamp() == 12.5);
    CHECK(key.GetDeviceId() == 3);
    CHECK(key.GetModifiers().IsSet(InputModifierBits::Control));
    CHECK(key.GetModifiers().IsSet(InputModifierBits::Shift));

    MouseButtonPressedEvent button(Mouse::ButtonLeft, glm::vec2(12.0f, 34.0f), modifiers, 14.0, 2);
    CHECK(button.GetPosition() == glm::vec2(12.0f, 34.0f));
    CHECK(button.GetModifiers().IsSet(InputModifierBits::Control));
    CHECK(button.GetTimestamp() == 14.0);
    CHECK(button.GetDeviceId() == 2);

    MouseMovedEvent moved(20.0f, 30.0f, glm::vec2(3.0f, -2.0f));
    CHECK(moved.GetDelta() == glm::vec2(3.0f, -2.0f));
}
