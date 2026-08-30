#include "cwpch.h"

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{

    ScriptComponentBase::ScriptComponentBase(MonoObject* instance) : ScriptSceneObjectBase(instance) {}
    ScriptComponent::ScriptComponent(MonoObject* instance) : ScriptObject(instance) {}

    void ScriptComponent::InitRuntimeData() {}

} // namespace Crowny
