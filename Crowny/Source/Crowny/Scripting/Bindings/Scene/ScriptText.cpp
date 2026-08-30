#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptText.h"

namespace Crowny
{
    ScriptText::ScriptText(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptText::InitRuntimeData() {}

} // namespace Crowny
