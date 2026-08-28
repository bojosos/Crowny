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
        static void Internal_GetShadowColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_SetShadowColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_GetShadowOffset(ScriptText* thisPtr, glm::vec2* offset);
        static void Internal_SetShadowOffset(ScriptText* thisPtr, glm::vec2* offset);
        static float Internal_GetShadowSoftness(ScriptText* thisPtr);
        static void Internal_SetShadowSoftness(ScriptText* thisPtr, float softness);

        static float Internal_GetSize(ScriptText* thisPtr);
        static void Internal_SetSize(ScriptText* thisPtr, float size);
        static bool Internal_GetAutoSize(ScriptText* thisPtr);
        static void Internal_SetAutoSize(ScriptText* thisPtr, bool autoSize);
        static float Internal_GetAutoSizeMin(ScriptText* thisPtr);
        static void Internal_SetAutoSizeMin(ScriptText* thisPtr, float size);
        static float Internal_GetAutoSizeMax(ScriptText* thisPtr);
        static void Internal_SetAutoSizeMax(ScriptText* thisPtr, float size);
        static void Internal_GetLayoutSize(ScriptText* thisPtr, glm::vec2* size);
        static void Internal_SetLayoutSize(ScriptText* thisPtr, glm::vec2* size);
        static bool Internal_GetWrapping(ScriptText* thisPtr);
        static void Internal_SetWrapping(ScriptText* thisPtr, bool wrapping);
        static TextWrapMode Internal_GetWrapMode(ScriptText* thisPtr);
        static void Internal_SetWrapMode(ScriptText* thisPtr, TextWrapMode mode);
        static TextOverflow Internal_GetOverflow(ScriptText* thisPtr);
        static void Internal_SetOverflow(ScriptText* thisPtr, TextOverflow overflow);
        static bool Internal_GetClipToBounds(ScriptText* thisPtr);
        static void Internal_SetClipToBounds(ScriptText* thisPtr, bool clip);
        static uint32_t Internal_GetMaxLines(ScriptText* thisPtr);
        static void Internal_SetMaxLines(ScriptText* thisPtr, uint32_t maxLines);
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
        static float Internal_GetParagraphSpacing(ScriptText* thisPtr);
        static void Internal_SetParagraphSpacing(ScriptText* thisPtr, float spacing);
        static uint32_t Internal_GetTabWidth(ScriptText* thisPtr);
        static void Internal_SetTabWidth(ScriptText* thisPtr, uint32_t width);
        static bool Internal_GetUseCustomDecorationColor(ScriptText* thisPtr);
        static void Internal_SetUseCustomDecorationColor(ScriptText* thisPtr, bool useCustomColor);
        static void Internal_GetDecorationColor(ScriptText* thisPtr, glm::vec4* color);
        static void Internal_SetDecorationColor(ScriptText* thisPtr, glm::vec4* color);
        static float Internal_GetDecorationThickness(ScriptText* thisPtr);
        static void Internal_SetDecorationThickness(ScriptText* thisPtr, float thickness);
        static float Internal_GetUnderlineOffset(ScriptText* thisPtr);
        static void Internal_SetUnderlineOffset(ScriptText* thisPtr, float offset);
        static float Internal_GetStrikethroughOffset(ScriptText* thisPtr);
        static void Internal_SetStrikethroughOffset(ScriptText* thisPtr, float offset);
        static bool Internal_GetUseKerning(ScriptText* thisPtr);
        static void Internal_SetUseKerning(ScriptText* thisPtr, bool useKerning);
        static int32_t Internal_GetSortingLayer(ScriptText* thisPtr);
        static void Internal_SetSortingLayer(ScriptText* thisPtr, int32_t value);
        static int32_t Internal_GetOrderInLayer(ScriptText* thisPtr);
        static void Internal_SetOrderInLayer(ScriptText* thisPtr, int32_t value);
    };
} // namespace Crowny
