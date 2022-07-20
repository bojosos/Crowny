#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptEntityBehaviour : public ScriptObject<ScriptEntityBehaviour, ScriptComponentBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "EntityBehaviour");

    private:
        virtual ScriptObjectBackupData BeginRefresh() override;
        virtual void EndRefresh(const ScriptObjectBackupData& data) override;
        virtual MonoObject* CreateManagedInstance(bool construct) override;
        virtual void ClearManagedInstance() override;
        virtual void OnManagedInstanceDeleted(bool assemblyRefresh) override;
        virtual void NotifyDestroyed() override;

        String m_Namespace;
        String m_TypeName;
        bool m_TypeMissing = false;
        Entity m_Entity;

    public:
        ScriptEntityBehaviour(MonoObject* instance, Entity entity);
    };
} // namespace Crowny