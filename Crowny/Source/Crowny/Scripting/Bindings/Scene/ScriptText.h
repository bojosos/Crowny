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

        static float Internal_GetSize(ScriptText* thisPtr);
        static void Internal_SetSize(ScriptText* thisPtr, float size);
        static bool Internal_GetAutoSize(ScriptText* thisPtr);
        static void Internal_SetAutoSize(ScriptText* thisPtr, bool autoSize);
        static bool Internal_GetWrapping(ScriptText* thisPtr);
        static void Internal_SetWrapping(ScriptText* thisPtr, bool wrapping);
        static TextOverflow Internal_GetOverflow(ScriptText* thisPtr);
        static void Internal_SetOverflow(ScriptText* thisPtr, TextOverflow overflow);
        static TextHorizontalAlignment Internal_GetHorizontalAlignment(ScriptText* thisPtr);
        static void Internal_SetHorizontalAlignment(ScriptText* thisPtr, TextHorizontalAlignment alignment);
        static TextVerticalAlignment Internal_GetVerticalAlignment(ScriptText* thisPtr);
        static void Internal_SetVerticalAlignment(ScriptText* thisPtr, TextVerticalAlignment alignment);
        static TextFontStyle Internal_GetFontStyle(ScriptText* thisPtr);
        static void Internal_SetFontStyle(ScriptText* thisPtr, TextFontStyle style);
        static float Internal_GetThickness(ScriptText* thisPtr);
        static void Internal_SetThickness(ScriptText* thisPtr, float thickness);
        static float Internal_GetCharacterSpacing(ScriptText* thisPtr);
        static void Internal_SetCharacterSpacing(ScriptText* thisPtr, float spacing);
        static float Internal_GetWordSpacing(ScriptText* thisPtr);
        static void Internal_SetWordSpacing(ScriptText* thisPtr, float spacing);
        static float Internal_GetLineSpacing(ScriptText* thisPtr);
        static void Internal_SetLineSpacing(ScriptText* thisPtr, float spacing);
        static bool Internal_GetUseKerning(ScriptText* thisPtr);
        static void Internal_SetUseKerning(ScriptText* thisPtr, bool useKerning);
    };
} // namespace Crowny