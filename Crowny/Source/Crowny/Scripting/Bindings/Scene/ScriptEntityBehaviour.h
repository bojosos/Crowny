#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ManagedScript;

    class ScriptEntityBehaviour : public ScriptObject<ScriptEntityBehaviour, ScriptComponentBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "EntityBehaviour");

    private:
        virtual MonoObject* CreateManagedInstance(bool construct) override;
        virtual void ClearManagedInstance() override;
        virtual void OnManagedInstanceDeleted(bool assemblyRefresh) override;
        virtual void NotifyDestroyed() override;

        String m_Assembly;
        String m_Namespace;
        String m_TypeName;
        uint64_t m_ScriptInstanceId = 0;

    public:
        ScriptEntityBehaviour(MonoObject* instance, Entity entity, const ManagedScript& script);
    };
} // namespace Crowny
