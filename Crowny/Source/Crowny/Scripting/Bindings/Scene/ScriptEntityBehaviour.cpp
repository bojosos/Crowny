#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity)
      : TScriptComponent(instance, entity)
    {
    }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny