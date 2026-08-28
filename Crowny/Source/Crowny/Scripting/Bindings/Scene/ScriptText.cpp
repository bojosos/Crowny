#include "cwpch.h"

#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/TextLayout.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptText.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

#include <limits>

namespace Crowny
{

    ScriptText::ScriptText(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptText::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetText", (void*)&Internal_GetText);
        MetaData.ScriptClass->AddInternalCall("Internal_SetText", (void*)&Internal_SetText);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFont", (void*)&Internal_GetFont);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFont", (void*)&Internal_SetFont);
        MetaData.ScriptClass->AddInternalCall("Internal_GetColor", (void*)&Internal_GetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColor", (void*)&Internal_SetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOutlineColor", (void*)&Internal_GetOutlineColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOutlineColor", (void*)&Internal_SetOutlineColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowColor", (void*)&Internal_GetShadowColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowColor", (void*)&Internal_SetShadowColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowOffset", (void*)&Internal_GetShadowOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowOffset", (void*)&Internal_SetShadowOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_GetShadowSoftness", (void*)&Internal_GetShadowSoftness);
        MetaData.ScriptClass->AddInternalCall("Internal_SetShadowSoftness", (void*)&Internal_SetShadowSoftness);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoSize", (void*)&Internal_GetAutoSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoSize", (void*)&Internal_SetAutoSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoSizeMin", (void*)&Internal_GetAutoSizeMin);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoSizeMin", (void*)&Internal_SetAutoSizeMin);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoSizeMax", (void*)&Internal_GetAutoSizeMax);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoSizeMax", (void*)&Internal_SetAutoSizeMax);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLayoutSize", (void*)&Internal_GetLayoutSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLayoutSize", (void*)&Internal_SetLayoutSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetWrapping", (void*)&Internal_GetWrapping);
        MetaData.ScriptClass->AddInternalCall("Internal_SetWrapping", (void*)&Internal_SetWrapping);
        MetaData.ScriptClass->AddInternalCall("Internal_GetWrapMode", (void*)&Internal_GetWrapMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetWrapMode", (void*)&Internal_SetWrapMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOverflow", (void*)&Internal_GetOverflow);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOverflow", (void*)&Internal_SetOverflow);
        MetaData.ScriptClass->AddInternalCall("Internal_GetClipToBounds", (void*)&Internal_GetClipToBounds);
        MetaData.ScriptClass->AddInternalCall("Internal_SetClipToBounds", (void*)&Internal_SetClipToBounds);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMaxLines", (void*)&Internal_GetMaxLines);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMaxLines", (void*)&Internal_SetMaxLines);
        MetaData.ScriptClass->AddInternalCall("Internal_GetHorizontalAlignment", (void*)&Internal_GetHorizontalAlignment);
        MetaData.ScriptClass->AddInternalCall("Internal_SetHorizontalAlignment", (void*)&Internal_SetHorizontalAlignment);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVerticalAlignment", (void*)&Internal_GetVerticalAlignment);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVerticalAlignment", (void*)&Internal_SetVerticalAlignment);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFontStyle", (void*)&Internal_GetFontStyle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFontStyle", (void*)&Internal_SetFontStyle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetThickness", (void*)&Internal_GetThickness);
        MetaData.ScriptClass->AddInternalCall("Internal_SetThickness", (void*)&Internal_SetThickness);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCharacterSpacing", (void*)&Internal_GetCharacterSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCharacterSpacing", (void*)&Internal_SetCharacterSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_GetWordSpacing", (void*)&Internal_GetWordSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_SetWordSpacing", (void*)&Internal_SetWordSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLineSpacing", (void*)&Internal_GetLineSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLineSpacing", (void*)&Internal_SetLineSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_GetParagraphSpacing", (void*)&Internal_GetParagraphSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_SetParagraphSpacing", (void*)&Internal_SetParagraphSpacing);
        MetaData.ScriptClass->AddInternalCall("Internal_GetTabWidth", (void*)&Internal_GetTabWidth);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTabWidth", (void*)&Internal_SetTabWidth);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUseCustomDecorationColor", (void*)&Internal_GetUseCustomDecorationColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUseCustomDecorationColor", (void*)&Internal_SetUseCustomDecorationColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetDecorationColor", (void*)&Internal_GetDecorationColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetDecorationColor", (void*)&Internal_SetDecorationColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetDecorationThickness", (void*)&Internal_GetDecorationThickness);
        MetaData.ScriptClass->AddInternalCall("Internal_SetDecorationThickness", (void*)&Internal_SetDecorationThickness);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUnderlineOffset", (void*)&Internal_GetUnderlineOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUnderlineOffset", (void*)&Internal_SetUnderlineOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_GetStrikethroughOffset", (void*)&Internal_GetStrikethroughOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetStrikethroughOffset", (void*)&Internal_SetStrikethroughOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_GetUseKerning", (void*)&Internal_GetUseKerning);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUseKerning", (void*)&Internal_SetUseKerning);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSortingLayer", (void*)&Internal_GetSortingLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSortingLayer", (void*)&Internal_SetSortingLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOrderInLayer", (void*)&Internal_GetOrderInLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOrderInLayer", (void*)&Internal_SetOrderInLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_HitTest", (void*)&Internal_HitTest);
    }

    MonoString* ScriptText::Internal_GetText(ScriptText* thisPtr) { return MonoUtils::ToMonoString(thisPtr->GetComponent().Text); }

    void ScriptText::Internal_SetText(ScriptText* thisPtr, MonoString* text) { thisPtr->GetComponent().Text = MonoUtils::FromMonoString(text); }

    MonoObject* ScriptText::Internal_GetFont(ScriptText* thisPtr)
    {
        ScriptAssetBase* asset = ScriptAssetManager::Get().GetScriptAsset(thisPtr->GetComponent().Font, true);
        if (asset != nullptr)
            return asset->GetManagedInstance();
        return nullptr;
    }

    void ScriptText::Internal_SetFont(ScriptText* thisPtr, MonoObject* font)
    {
        ScriptFont* nativeFont = ScriptFont::ToNative(font);
        if (nativeFont != nullptr)
            thisPtr->GetComponent().Font = nativeFont->GetHandle();
        else
            thisPtr->GetComponent().Font = AssetHandle<Font>();
    }

    void ScriptText::Internal_GetColor(ScriptText* thisPtr, glm::vec4* color) { *color = thisPtr->GetComponent().Color; }

    void ScriptText::Internal_SetColor(ScriptText* thisPtr, glm::vec4* color) { thisPtr->GetComponent().Color = *color; }

    void ScriptText::Internal_GetOutlineColor(ScriptText* thisPtr, glm::vec4* color) { *color = thisPtr->GetComponent().OutlineColor; }

    void ScriptText::Internal_SetOutlineColor(ScriptText* thisPtr, glm::vec4* color) { thisPtr->GetComponent().OutlineColor = *color; }

    void ScriptText::Internal_GetShadowColor(ScriptText* thisPtr, glm::vec4* color) { *color = thisPtr->GetComponent().ShadowColor; }

    void ScriptText::Internal_SetShadowColor(ScriptText* thisPtr, glm::vec4* color) { thisPtr->GetComponent().ShadowColor = *color; }

    void ScriptText::Internal_GetShadowOffset(ScriptText* thisPtr, glm::vec2* offset) { *offset = thisPtr->GetComponent().ShadowOffset; }

    void ScriptText::Internal_SetShadowOffset(ScriptText* thisPtr, glm::vec2* offset) { thisPtr->GetComponent().ShadowOffset = *offset; }

    float ScriptText::Internal_GetShadowSoftness(ScriptText* thisPtr) { return thisPtr->GetComponent().ShadowSoftness; }

    void ScriptText::Internal_SetShadowSoftness(ScriptText* thisPtr, float softness)
    {
        thisPtr->GetComponent().ShadowSoftness = std::max(0.0f, softness);
    }

    float ScriptText::Internal_GetSize(ScriptText* thisPtr) { return thisPtr->GetComponent().Size; }

    void ScriptText::Internal_SetSize(ScriptText* thisPtr, float size) { thisPtr->GetComponent().Size = size; }

    bool ScriptText::Internal_GetAutoSize(ScriptText* thisPtr) { return thisPtr->GetComponent().AutoSize; }

    void ScriptText::Internal_SetAutoSize(ScriptText* thisPtr, bool autoSize) { thisPtr->GetComponent().AutoSize = autoSize; }

    float ScriptText::Internal_GetAutoSizeMin(ScriptText* thisPtr) { return thisPtr->GetComponent().AutoSizeMin; }

    void ScriptText::Internal_SetAutoSizeMin(ScriptText* thisPtr, float size)
    {
        auto& component = thisPtr->GetComponent();
        component.AutoSizeMin = std::max(0.0f, size);
        component.AutoSizeMax = std::max(component.AutoSizeMin, component.AutoSizeMax);
    }

    float ScriptText::Internal_GetAutoSizeMax(ScriptText* thisPtr) { return thisPtr->GetComponent().AutoSizeMax; }

    void ScriptText::Internal_SetAutoSizeMax(ScriptText* thisPtr, float size)
    {
        auto& component = thisPtr->GetComponent();
        component.AutoSizeMax = std::max(0.0f, size);
        component.AutoSizeMin = std::min(component.AutoSizeMin, component.AutoSizeMax);
    }

    void ScriptText::Internal_GetLayoutSize(ScriptText* thisPtr, glm::vec2* size) { *size = thisPtr->GetComponent().LayoutSize; }

    void ScriptText::Internal_SetLayoutSize(ScriptText* thisPtr, glm::vec2* size)
    {
        thisPtr->GetComponent().LayoutSize = glm::max(*size, glm::vec2(0.0f));
    }

    bool ScriptText::Internal_GetWrapping(ScriptText* thisPtr) { return thisPtr->GetComponent().Wrapping; }

    void ScriptText::Internal_SetWrapping(ScriptText* thisPtr, bool wrapping) { thisPtr->GetComponent().Wrapping = wrapping; }

    TextWrapMode ScriptText::Internal_GetWrapMode(ScriptText* thisPtr) { return thisPtr->GetComponent().WrapMode; }

    void ScriptText::Internal_SetWrapMode(ScriptText* thisPtr, TextWrapMode mode) { thisPtr->GetComponent().WrapMode = mode; }

    TextOverflow ScriptText::Internal_GetOverflow(ScriptText* thisPtr) { return thisPtr->GetComponent().Overflow; }

    void ScriptText::Internal_SetOverflow(ScriptText* thisPtr, TextOverflow overflow) { thisPtr->GetComponent().Overflow = overflow; }

    bool ScriptText::Internal_GetClipToBounds(ScriptText* thisPtr) { return thisPtr->GetComponent().ClipToBounds; }

    void ScriptText::Internal_SetClipToBounds(ScriptText* thisPtr, bool clip) { thisPtr->GetComponent().ClipToBounds = clip; }

    uint32_t ScriptText::Internal_GetMaxLines(ScriptText* thisPtr) { return thisPtr->GetComponent().MaxLines; }

    void ScriptText::Internal_SetMaxLines(ScriptText* thisPtr, uint32_t maxLines) { thisPtr->GetComponent().MaxLines = maxLines; }

    TextHorizontalAlignment ScriptText::Internal_GetHorizontalAlignment(ScriptText* thisPtr) { return thisPtr->GetComponent().HorizontalAlignment; }

    void ScriptText::Internal_SetHorizontalAlignment(ScriptText* thisPtr, TextHorizontalAlignment alignment)
    {
        thisPtr->GetComponent().HorizontalAlignment = alignment;
    }

    TextVerticalAlignment ScriptText::Internal_GetVerticalAlignment(ScriptText* thisPtr) { return thisPtr->GetComponent().VerticalAlignment; }

    void ScriptText::Internal_SetVerticalAlignment(ScriptText* thisPtr, TextVerticalAlignment alignment)
    {
        thisPtr->GetComponent().VerticalAlignment = alignment;
    }

    TextFontStyle ScriptText::Internal_GetFontStyle(ScriptText* thisPtr) { return thisPtr->GetComponent().FontStyle; }

    void ScriptText::Internal_SetFontStyle(ScriptText* thisPtr, TextFontStyle style) { thisPtr->GetComponent().FontStyle = style; }

    float ScriptText::Internal_GetThickness(ScriptText* thisPtr) { return thisPtr->GetComponent().Thickness; }

    void ScriptText::Internal_SetThickness(ScriptText* thisPtr, float thickness) { thisPtr->GetComponent().Thickness = thickness; }

    float ScriptText::Internal_GetCharacterSpacing(ScriptText* thisPtr) { return thisPtr->GetComponent().CharacterSpacing; }

    void ScriptText::Internal_SetCharacterSpacing(ScriptText* thisPtr, float spacing) { thisPtr->GetComponent().CharacterSpacing = spacing; }

    float ScriptText::Internal_GetWordSpacing(ScriptText* thisPtr) { return thisPtr->GetComponent().WordSpacing; }

    void ScriptText::Internal_SetWordSpacing(ScriptText* thisPtr, float spacing) { thisPtr->GetComponent().WordSpacing = spacing; }

    float ScriptText::Internal_GetLineSpacing(ScriptText* thisPtr) { return thisPtr->GetComponent().LineSpacing; }

    void ScriptText::Internal_SetLineSpacing(ScriptText* thisPtr, float spacing) { thisPtr->GetComponent().LineSpacing = spacing; }

    float ScriptText::Internal_GetParagraphSpacing(ScriptText* thisPtr) { return thisPtr->GetComponent().ParagraphSpacing; }

    void ScriptText::Internal_SetParagraphSpacing(ScriptText* thisPtr, float spacing) { thisPtr->GetComponent().ParagraphSpacing = spacing; }

    uint32_t ScriptText::Internal_GetTabWidth(ScriptText* thisPtr) { return thisPtr->GetComponent().TabWidth; }

    void ScriptText::Internal_SetTabWidth(ScriptText* thisPtr, uint32_t width) { thisPtr->GetComponent().TabWidth = std::max(1u, width); }

    bool ScriptText::Internal_GetUseCustomDecorationColor(ScriptText* thisPtr) { return thisPtr->GetComponent().UseCustomDecorationColor; }

    void ScriptText::Internal_SetUseCustomDecorationColor(ScriptText* thisPtr, bool useCustomColor)
    {
        thisPtr->GetComponent().UseCustomDecorationColor = useCustomColor;
    }

    void ScriptText::Internal_GetDecorationColor(ScriptText* thisPtr, glm::vec4* color) { *color = thisPtr->GetComponent().DecorationColor; }

    void ScriptText::Internal_SetDecorationColor(ScriptText* thisPtr, glm::vec4* color) { thisPtr->GetComponent().DecorationColor = *color; }

    float ScriptText::Internal_GetDecorationThickness(ScriptText* thisPtr) { return thisPtr->GetComponent().DecorationThickness; }

    void ScriptText::Internal_SetDecorationThickness(ScriptText* thisPtr, float thickness)
    {
        thisPtr->GetComponent().DecorationThickness = std::max(0.0f, thickness);
    }

    float ScriptText::Internal_GetUnderlineOffset(ScriptText* thisPtr) { return thisPtr->GetComponent().UnderlineOffset; }

    void ScriptText::Internal_SetUnderlineOffset(ScriptText* thisPtr, float offset) { thisPtr->GetComponent().UnderlineOffset = offset; }

    float ScriptText::Internal_GetStrikethroughOffset(ScriptText* thisPtr) { return thisPtr->GetComponent().StrikethroughOffset; }

    void ScriptText::Internal_SetStrikethroughOffset(ScriptText* thisPtr, float offset) { thisPtr->GetComponent().StrikethroughOffset = offset; }

    bool ScriptText::Internal_GetUseKerning(ScriptText* thisPtr) { return thisPtr->GetComponent().UseKerning; }

    void ScriptText::Internal_SetUseKerning(ScriptText* thisPtr, bool useKerning) { thisPtr->GetComponent().UseKerning = useKerning; }

    int32_t ScriptText::Internal_GetSortingLayer(ScriptText* thisPtr) { return thisPtr->GetComponent().SortingLayer; }

    void ScriptText::Internal_SetSortingLayer(ScriptText* thisPtr, int32_t value) { thisPtr->GetComponent().SortingLayer = value; }

    int32_t ScriptText::Internal_GetOrderInLayer(ScriptText* thisPtr) { return thisPtr->GetComponent().OrderInLayer; }

    void ScriptText::Internal_SetOrderInLayer(ScriptText* thisPtr, int32_t value) { thisPtr->GetComponent().OrderInLayer = value; }

    uint32_t ScriptText::Internal_HitTest(ScriptText* thisPtr, glm::vec2* localPosition)
    {
        if (thisPtr == nullptr || localPosition == nullptr)
            return 0;

        const TextComponent& component = thisPtr->GetComponent();
        AssetHandle<Font> font = component.Font;
        if (!font)
            font = FontManager::GetDefaultFont();
        if (!font || !font->IsValid())
            return 0;

        TextLayoutScratch scratch;
        const TextLayoutResult layout = TextLayout::Build(component, *font, scratch);
        const TextHitTestResult hit = TextLayout::HitTest(layout, *localPosition);
        return hit.Valid ? static_cast<uint32_t>(std::min(hit.SourceByteOffset, size_t(std::numeric_limits<uint32_t>::max()))) : 0;
    }

} // namespace Crowny
