#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptSpriteRenderer : public TScriptComponent<ScriptSpriteRenderer, SpriteRendererComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SpriteRendererComponent")

        ScriptSpriteRenderer(MonoObject* instance, Entity entity);

    private:
        static MonoObject* Internal_GetTexture(ScriptSpriteRenderer* thisPtr);
        static void Internal_SetTexture(ScriptSpriteRenderer* thisPtr, MonoObject* texture);
        static void Internal_GetColor(ScriptSpriteRenderer* thisPtr, glm::vec4* value);
        static void Internal_SetColor(ScriptSpriteRenderer* thisPtr, glm::vec4* value);
        static int32_t Internal_GetSortingLayer(ScriptSpriteRenderer* thisPtr);
        static void Internal_SetSortingLayer(ScriptSpriteRenderer* thisPtr, int32_t value);
        static int32_t Internal_GetOrderInLayer(ScriptSpriteRenderer* thisPtr);
        static void Internal_SetOrderInLayer(ScriptSpriteRenderer* thisPtr, int32_t value);
    };
} // namespace Crowny
