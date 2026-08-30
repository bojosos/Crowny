#pragma once

#include "Crowny/Common/HashedString.h"
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
        /** Callback references are valid only for the call and must not escape. Return true only after an actual edit. */
        template <typename Callback> bool EditContexts(Callback&& callback)
        {
            if (!std::invoke(std::forward<Callback>(callback), m_Contexts))
                return false;

            EnsureStableIds();
            return true;
        }
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

    private:
        void EnsureStableIds();

        struct ActionState
        {
            InputActionValue Current;
            bool Pressed = false;
            bool Released = false;
            float PressThreshold = 0.5f;
            uint64_t LastResolvedGeneration = 0;
        };

        Vector<InputContext> m_Contexts;
        UnorderedMap<String, ActionState, StringHash, StringEqual> m_ActionStates;
        UnorderedMap<UUID, InputBinding> m_RebindOverrides;
        Vector<size_t> m_OrderedContextIndices;
        Vector<uint64_t> m_ConsumedControls;
        Vector<uint64_t> m_ContextConsumedControls;
        uint64_t m_UpdateGeneration = 0;
        bool m_KeyboardCaptured = false;
        bool m_PointerCaptured = false;
    };
} // namespace Crowny
