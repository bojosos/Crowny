#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptSpriteRenderer.h"

namespace Crowny
{
    ScriptSpriteRenderer::ScriptSpriteRenderer(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptSpriteRenderer::InitRuntimeData() {}

} // namespace Crowny
