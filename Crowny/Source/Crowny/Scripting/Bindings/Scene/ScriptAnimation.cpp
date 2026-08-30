#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAnimation.h"

namespace Crowny
{
    ScriptAnimation::ScriptAnimation(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptAnimation::InitRuntimeData() {}

} // namespace Crowny
