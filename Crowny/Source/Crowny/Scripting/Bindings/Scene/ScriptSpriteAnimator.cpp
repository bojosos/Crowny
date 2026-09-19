#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptSpriteAnimator.h"

namespace Crowny
{
    ScriptSpriteAnimator::ScriptSpriteAnimator(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}
    void ScriptSpriteAnimator::InitRuntimeData() {}
} // namespace Crowny
