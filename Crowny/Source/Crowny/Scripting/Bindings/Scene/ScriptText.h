#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptText : public TScriptComponent<ScriptText, TextComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Text");

        ScriptText(MonoObject* instance, Entity entity);

    private:
        static MonoString* Internal_GetText(ScriptText* thisPtr);
        static void Internal_SetText(ScriptText* thisPtr, MonoString* text);
        static MonoObject* Internal_GetFont(ScriptText* thisPtr);
        static void Internal_SetFont(ScriptText* thisPtr, MonoObject* font);
        static void Internal_GetColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_SetColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_GetOutlineColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_SetOutlineColor(ScriptText* thisPtr, glm::vec4* color);
    };
} // namespace Crowny