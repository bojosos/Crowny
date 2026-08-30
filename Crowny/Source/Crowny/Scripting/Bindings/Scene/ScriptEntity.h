#pragma once

#include "Crowny/Scripting/ScriptSceneObject.h"

namespace Crowny
{
    class ScriptEntity : public ScriptObject<ScriptEntity, ScriptSceneObjectBase>
    {

    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Entity")

        ScriptEntity(MonoObject* instance, Entity entity);

    private:
        MonoObject* CreateManagedInstance(bool construct) override;
    };
} // namespace Crowny
