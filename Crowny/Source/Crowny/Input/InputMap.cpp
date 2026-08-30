#include "cwpch.h"

#include "Crowny/Input/InputMap.h"

#include <algorithm>
#include <cmath>

namespace Crowny
{
    namespace
    {
        constexpr float MIN_THRESHOLD = 0.0001f;

        class RawInputStateReader final : public InputStateReader
        {
        public:
            DigitalInputState ReadKey(KeyCode key) const override { return { Input::IsKeyPressed(key), Input::IsKeyDown(key), Input::IsKeyUp(key) }; }

            DigitalInputState ReadMouseButton(MouseCode button) const override
            {
                return { Input::IsMouseButtonPressed(button), Input::IsMouseButtonDown(button), Input::IsMouseButtonUp(button) };
            }

            bool IsGamepadConnected(uint32_t gamepadIndex) const override { return Input::IsGamepadConnected(gamepadIndex); }

            DigitalInputState ReadGamepadButton(uint32_t gamepadIndex, GamepadButtonCode button) const override
            {
                return { Input::IsGamepadButtonPressed(gamepadIndex, button), Input::IsGamepadButtonDown(gamepadIndex, button),
                         Input::IsGamepadButtonUp(gamepadIndex, button) };
            }

            float ReadGamepadAxis(uint32_t gamepadIndex, GamepadAxisCode axis) const override { return Input::GetGamepadAxis(gamepadIndex, axis); }
        };

        struct BindingResult
        {
            glm::vec2 Value = glm::vec2(0.0f);
            bool Pressed = false;
            bool Released = false;
            bool Active = false;
        };

        uint64_t GetControlId(const InputBinding& binding)
        {
            return static_cast<uint64_t>(binding.Code) | (static_cast<uint64_t>(binding.GamepadIndex) << 32) |
                   (static_cast<uint64_t>(binding.Device) << 56);
        }

        bool IsHeld(const DigitalInputState& state) { return state.Held; }

        bool AreModifiersHeld(const InputStateReader& input, InputModifiers modifiers)
        {
            const auto eitherHeld = [&input](KeyCode left, KeyCode right) { return IsHeld(input.ReadKey(left)) || IsHeld(input.ReadKey(right)); };

            if (modifiers.IsSet(InputModifierBits::Shift) && !eitherHeld(Key::LeftShift, Key::RightShift))
                return false;
            if (modifiers.IsSet(InputModifierBits::Control) && !eitherHeld(Key::LeftControl, Key::RightControl))
                return false;
            if (modifiers.IsSet(InputModifierBits::Alt) && !eitherHeld(Key::LeftAlt, Key::RightAlt))
                return false;
            if (modifiers.IsSet(InputModifierBits::Super) && !eitherHeld(Key::LeftSuper, Key::RightSuper))
                return false;
            return true;
        }

        float ApplyDeadZone(float value, float deadZone)
        {
            const float clampedDeadZone = std::clamp(deadZone, 0.0f, 0.99f);
            const float magnitude = std::abs(value);
            if (magnitude <= clampedDeadZone)
                return 0.0f;
            return std::copysign((magnitude - clampedDeadZone) / (1.0f - clampedDeadZone), value);
        }

        glm::vec2 MapBindingValue(InputActionType type, InputBindingPart part, float value)
        {
            const float magnitude = std::abs(value);
            switch (type)
            {
            case InputActionType::Button:
                return { magnitude, 0.0f };
            case InputActionType::Axis1D:
                if (part == InputBindingPart::Negative || part == InputBindingPart::Left || part == InputBindingPart::Down)
                    return { -magnitude, 0.0f };
                if (part == InputBindingPart::Positive || part == InputBindingPart::Right || part == InputBindingPart::Up)
                    return { magnitude, 0.0f };
                return { value, 0.0f };
            case InputActionType::Axis2D:
                switch (part)
                {
                case InputBindingPart::Up:
                    return { 0.0f, magnitude };
                case InputBindingPart::Down:
                    return { 0.0f, -magnitude };
                case InputBindingPart::Left:
                    return { -magnitude, 0.0f };
                case InputBindingPart::Right:
                    return { magnitude, 0.0f };
                case InputBindingPart::Y:
                    return { 0.0f, value };
                case InputBindingPart::Whole:
                case InputBindingPart::Positive:
                case InputBindingPart::Negative:
                case InputBindingPart::X:
                default:
                    return { value, 0.0f };
                }
            }
            return glm::vec2(0.0f);
        }

        BindingResult EvaluateBinding(const InputBinding& binding, InputActionType type, const InputStateReader& input, bool keyboardCaptured,
                                      bool pointerCaptured)
        {
            BindingResult result;
            if ((binding.Device == InputBindingDevice::Keyboard && keyboardCaptured) ||
                (binding.Device == InputBindingDevice::Mouse && pointerCaptured) || !AreModifiersHeld(input, binding.Modifiers))
                return result;
            if ((binding.Device == InputBindingDevice::GamepadButton || binding.Device == InputBindingDevice::GamepadAxis) &&
                !input.IsGamepadConnected(binding.GamepadIndex))
                return result;

            float value = 0.0f;
            switch (binding.Device)
            {
            case InputBindingDevice::Keyboard: {
                const DigitalInputState state = input.ReadKey(static_cast<KeyCode>(binding.Code));
                value = state.Held ? 1.0f : 0.0f;
                result.Pressed = state.Pressed;
                result.Released = state.Released;
                break;
            }
            case InputBindingDevice::Mouse: {
                const DigitalInputState state = input.ReadMouseButton(static_cast<MouseCode>(binding.Code));
                value = state.Held ? 1.0f : 0.0f;
                result.Pressed = state.Pressed;
                result.Released = state.Released;
                break;
            }
            case InputBindingDevice::GamepadButton: {
                const DigitalInputState state = input.ReadGamepadButton(binding.GamepadIndex, static_cast<GamepadButtonCode>(binding.Code));
                value = state.Held ? 1.0f : 0.0f;
                result.Pressed = state.Pressed;
                result.Released = state.Released;
                break;
            }
            case InputBindingDevice::GamepadAxis:
                value = input.ReadGamepadAxis(binding.GamepadIndex, static_cast<GamepadAxisCode>(binding.Code));
                if (binding.Code == GamepadAxis::LeftTrigger || binding.Code == GamepadAxis::RightTrigger)
                    value = value * 0.5f + 0.5f;
                value = ApplyDeadZone(value, binding.DeadZone);
                break;
            }

            value *= binding.Scale * (binding.Invert ? -1.0f : 1.0f);
            result.Value = MapBindingValue(type, binding.Part, value);
            result.Active = glm::length(result.Value) > MIN_THRESHOLD || result.Pressed || result.Released;
            return result;
        }

        float GetMagnitude(const InputActionValue& value)
        {
            switch (value.Type)
            {
            case InputActionType::Button:
            case InputActionType::Axis1D:
                return std::abs(value.Value.x);
            case InputActionType::Axis2D:
                return glm::length(value.Value);
            }
            return 0.0f;
        }
    } // namespace

    bool InputActionValue::AsButton(float threshold) const { return GetMagnitude(*this) >= std::max(threshold, MIN_THRESHOLD); }

    InputMap::InputMap(Vector<InputContext> contexts) : m_Contexts(std::move(contexts)) { EnsureStableIds(); }

    void InputMap::SetContexts(Vector<InputContext> contexts)
    {
        m_Contexts = std::move(contexts);
        EnsureStableIds();
        m_ActionStates.clear();

        UnorderedSet<UUID> liveBindings;
        for (const InputContext& context : m_Contexts)
        {
            for (const InputAction& action : context.Actions)
            {
                for (const InputBinding& binding : action.Bindings)
                    liveBindings.insert(binding.Id);
            }
        }

        for (auto it = m_RebindOverrides.begin(); it != m_RebindOverrides.end();)
        {
            if (liveBindings.find(it->first) == liveBindings.end())
                it = m_RebindOverrides.erase(it);
            else
                ++it;
        }
    }

    void InputMap::Update()
    {
        static const RawInputStateReader input;
        Update(input);
    }

    void InputMap::Update(const InputStateReader& input)
    {
        m_UpdateGeneration++;
        if (m_UpdateGeneration == 0)
        {
            for (auto& [name, state] : m_ActionStates)
                state.LastResolvedGeneration = 0;
            m_UpdateGeneration = 1;
        }

        m_OrderedContextIndices.clear();
        for (size_t index = 0; index < m_Contexts.size(); index++)
        {
            if (m_Contexts[index].Enabled)
                m_OrderedContextIndices.push_back(index);
        }
        std::sort(m_OrderedContextIndices.begin(), m_OrderedContextIndices.end(), [this](size_t lhs, size_t rhs) {
            const int32_t lhsPriority = m_Contexts[lhs].Priority;
            const int32_t rhsPriority = m_Contexts[rhs].Priority;
            return lhsPriority != rhsPriority ? lhsPriority > rhsPriority : lhs < rhs;
        });

        m_ConsumedControls.clear();
        for (const size_t contextIndex : m_OrderedContextIndices)
        {
            const InputContext& context = m_Contexts[contextIndex];
            m_ContextConsumedControls.clear();
            for (const InputAction& action : context.Actions)
            {
                if (action.Name.empty())
                    continue;

                auto [stateIter, inserted] = m_ActionStates.try_emplace(action.Name);
                if (stateIter->second.LastResolvedGeneration == m_UpdateGeneration)
                    continue;

                const bool wasHeld =
                  !inserted && stateIter->second.Current.AsButton(stateIter->second.PressThreshold);
                ActionState state;
                state.Current.Type = action.Type;
                state.PressThreshold = std::clamp(action.PressThreshold, MIN_THRESHOLD, 1.0f);
                state.LastResolvedGeneration = m_UpdateGeneration;
                bool bindingPressed = false;
                bool bindingReleased = false;

                for (const InputBinding& authoredBinding : action.Bindings)
                {
                    const InputBinding& binding = *GetEffectiveBinding(authoredBinding);
                    const uint64_t controlId = GetControlId(binding);
                    if (std::find(m_ConsumedControls.begin(), m_ConsumedControls.end(), controlId) != m_ConsumedControls.end())
                        continue;

                    const BindingResult bindingResult = EvaluateBinding(binding, action.Type, input, m_KeyboardCaptured, m_PointerCaptured);
                    state.Current.Value += bindingResult.Value;
                    bindingPressed |= bindingResult.Pressed;
                    bindingReleased |= bindingResult.Released;
                    if (context.ConsumeInput && bindingResult.Active &&
                        std::find(m_ContextConsumedControls.begin(), m_ContextConsumedControls.end(), controlId) ==
                          m_ContextConsumedControls.end())
                        m_ContextConsumedControls.push_back(controlId);
                }

                state.Current.Value.x = std::clamp(state.Current.Value.x, -1.0f, 1.0f);
                state.Current.Value.y = std::clamp(state.Current.Value.y, -1.0f, 1.0f);
                if (action.Type == InputActionType::Button)
                    state.Current.Value.x = state.Current.AsButton(state.PressThreshold) ? 1.0f : 0.0f;
                else if (action.Type == InputActionType::Axis2D && glm::length(state.Current.Value) > 1.0f)
                    state.Current.Value = glm::normalize(state.Current.Value);

                const bool held = state.Current.AsButton(state.PressThreshold);
                state.Pressed = (!wasHeld && held) || bindingPressed;
                state.Released = (wasHeld && !held) || bindingReleased;
                stateIter->second = state;
            }
            m_ConsumedControls.insert(m_ConsumedControls.end(), m_ContextConsumedControls.begin(), m_ContextConsumedControls.end());
        }

        for (auto stateIter = m_ActionStates.begin(); stateIter != m_ActionStates.end();)
        {
            ActionState& state = stateIter->second;
            if (state.LastResolvedGeneration == m_UpdateGeneration)
            {
                ++stateIter;
                continue;
            }

            if (!state.Current.AsButton(state.PressThreshold))
            {
                stateIter = m_ActionStates.erase(stateIter);
                continue;
            }

            state.Current.Value = glm::vec2(0.0f);
            state.Pressed = false;
            state.Released = true;
            ++stateIter;
        }
    }

    void InputMap::SetCapture(bool keyboardCaptured, bool pointerCaptured)
    {
        m_KeyboardCaptured = keyboardCaptured;
        m_PointerCaptured = pointerCaptured;
    }

    InputActionValue InputMap::GetValue(StringView actionName) const
    {
        const auto state = m_ActionStates.find(actionName);
        return state == m_ActionStates.end() ? InputActionValue{} : state->second.Current;
    }

    float InputMap::GetAxis1D(StringView actionName) const { return GetValue(actionName).AsAxis1D(); }

    glm::vec2 InputMap::GetAxis2D(StringView actionName) const { return GetValue(actionName).AsAxis2D(); }

    bool InputMap::IsPressed(StringView actionName) const
    {
        const auto state = m_ActionStates.find(actionName);
        return state != m_ActionStates.end() && state->second.Pressed;
    }

    bool InputMap::IsHeld(StringView actionName) const
    {
        const auto state = m_ActionStates.find(actionName);
        return state != m_ActionStates.end() && state->second.Current.AsButton(state->second.PressThreshold);
    }

    bool InputMap::IsReleased(StringView actionName) const
    {
        const auto state = m_ActionStates.find(actionName);
        return state != m_ActionStates.end() && state->second.Released;
    }

    bool InputMap::SetContextEnabled(StringView contextName, bool enabled)
    {
        const auto context =
          std::find_if(m_Contexts.begin(), m_Contexts.end(), [contextName](const InputContext& candidate) { return candidate.Name == contextName; });
        if (context == m_Contexts.end())
            return false;
        context->Enabled = enabled;
        return true;
    }

    bool InputMap::Rebind(const UUID& bindingId, const InputBinding& binding)
    {
        if (bindingId.Empty())
            return false;

        for (const InputContext& context : m_Contexts)
        {
            for (const InputAction& action : context.Actions)
            {
                for (const InputBinding& candidate : action.Bindings)
                {
                    if (candidate.Id != bindingId)
                        continue;
                    InputBinding overrideBinding = binding;
                    overrideBinding.Id = bindingId;
                    m_RebindOverrides[bindingId] = overrideBinding;
                    return true;
                }
            }
        }
        return false;
    }

    bool InputMap::Rebind(StringView contextName, StringView actionName, size_t bindingIndex, const InputBinding& binding)
    {
        for (const InputContext& context : m_Contexts)
        {
            if (context.Name != contextName)
                continue;
            for (const InputAction& action : context.Actions)
            {
                if (action.Name == actionName && bindingIndex < action.Bindings.size())
                    return Rebind(action.Bindings[bindingIndex].Id, binding);
            }
        }
        return false;
    }

    bool InputMap::ClearRebind(const UUID& bindingId) { return m_RebindOverrides.erase(bindingId) != 0; }

    void InputMap::ClearAllRebinds() { m_RebindOverrides.clear(); }

    const InputBinding* InputMap::GetEffectiveBinding(const InputBinding& authoredBinding) const
    {
        const auto binding = m_RebindOverrides.find(authoredBinding.Id);
        return binding == m_RebindOverrides.end() ? &authoredBinding : &binding->second;
    }

    void InputMap::EnsureStableIds()
    {
        UnorderedSet<UUID> ids;
        const auto assignUniqueId = [&ids](UUID& id) {
            if (!id.Empty() && ids.insert(id).second)
                return;
            do
            {
                id = UuidGenerator::Generate();
            } while (id.Empty() || !ids.insert(id).second);
        };

        for (InputContext& context : m_Contexts)
        {
            assignUniqueId(context.Id);
            for (InputAction& action : context.Actions)
            {
                assignUniqueId(action.Id);
                action.PressThreshold = std::clamp(action.PressThreshold, MIN_THRESHOLD, 1.0f);
                for (InputBinding& binding : action.Bindings)
                {
                    assignUniqueId(binding.Id);
                    binding.DeadZone = std::clamp(binding.DeadZone, 0.0f, 0.99f);
                }
            }
        }
    }
} // namespace Crowny
