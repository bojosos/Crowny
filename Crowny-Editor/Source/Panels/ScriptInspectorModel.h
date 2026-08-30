#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/ScriptRuntime.h"

namespace Crowny
{
    // Inspector access is deliberately state-only. Reading an inspector must not
    // create a managed object or run a user script constructor.
    class ScriptInspectorModel
    {
    public:
        explicit ScriptInspectorModel(ManagedScript& script) : m_Script(script), m_State(ScriptRuntime::CaptureState(script)) {}

        ScriptState& GetState() { return m_State; }
        const ScriptState& GetState() const { return m_State; }
        bool Commit() { return ScriptRuntime::ApplyState(m_Script, m_State); }

    private:
        ManagedScript& m_Script;
        ScriptState m_State;
    };
} // namespace Crowny
