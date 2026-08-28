#pragma once

#include "Crowny/Input/InputAction.h"

namespace Crowny
{
    /**
     * Evaluates named actions from ordered input contexts. If enabled contexts
     * define the same action, the highest-priority context wins. An active
     * consuming context also prevents lower-priority bindings from reading the
     * same physical control.
     */
    class InputMap
    {
    public:
        InputMap() = default;
        explicit InputMap(Vector<InputContext> contexts);

        const Vector<InputContext>& GetContexts() const { return m_Contexts; }
        Vector<InputContext>& GetContexts() { return m_Contexts; }
        void SetContexts(Vector<InputContext> contexts);

        void Update();
        void Update(const InputStateReader& input);

        void SetCapture(bool keyboardCaptured, bool pointerCaptured);
        bool IsKeyboardCaptured() const { return m_KeyboardCaptured; }
        bool IsPointerCaptured() const { return m_PointerCaptured; }

        InputActionValue GetValue(StringView actionName) const;
        float GetAxis1D(StringView actionName) const;
        glm::vec2 GetAxis2D(StringView actionName) const;
        bool IsPressed(StringView actionName) const;
        bool IsHeld(StringView actionName) const;
        bool IsReleased(StringView actionName) const;

        bool SetContextEnabled(StringView contextName, bool enabled);
        bool Rebind(const UUID& bindingId, const InputBinding& binding);
        bool Rebind(StringView contextName, StringView actionName, size_t bindingIndex, const InputBinding& binding);
        bool ClearRebind(const UUID& bindingId);
        void ClearAllRebinds();
        const InputBinding* GetEffectiveBinding(const InputBinding& authoredBinding) const;

        void EnsureStableIds();

    private:
        struct ActionState
        {
            InputActionValue Current;
            bool Pressed = false;
            bool Released = false;
            float PressThreshold = 0.5f;
        };

        Vector<InputContext> m_Contexts;
        UnorderedMap<String, ActionState> m_ActionStates;
        UnorderedMap<UUID, InputBinding> m_RebindOverrides;
        bool m_KeyboardCaptured = false;
        bool m_PointerCaptured = false;
    };
} // namespace Crowny
