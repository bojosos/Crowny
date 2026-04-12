#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptText.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

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
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAutoSize", (void*)&Internal_GetAutoSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAutoSize", (void*)&Internal_SetAutoSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetWrapping", (void*)&Internal_GetWrapping);
        MetaData.ScriptClass->AddInternalCall("Internal_SetWrapping", (void*)&Internal_SetWrapping);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOverflow", (void*)&Internal_GetOverflow);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOverflow", (void*)&Internal_SetOverflow);
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
        MetaData.ScriptClass->AddInternalCall("Internal_GetUseKerning", (void*)&Internal_GetUseKerning);
        MetaData.ScriptClass->AddInternalCall("Internal_SetUseKerning", (void*)&Internal_SetUseKerning);
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

    float ScriptText::Internal_GetSize(ScriptText* thisPtr) { return thisPtr->GetComponent().Size; }

    void ScriptText::Internal_SetSize(ScriptText* thisPtr, float size) { thisPtr->GetComponent().Size = size; }

    bool ScriptText::Internal_GetAutoSize(ScriptText* thisPtr) { return thisPtr->GetComponent().AutoSize; }

    void ScriptText::Internal_SetAutoSize(ScriptText* thisPtr, bool autoSize) { thisPtr->GetComponent().AutoSize = autoSize; }

    bool ScriptText::Internal_GetWrapping(ScriptText* thisPtr) { return thisPtr->GetComponent().Wrapping; }

    void ScriptText::Internal_SetWrapping(ScriptText* thisPtr, bool wrapping) { thisPtr->GetComponent().Wrapping = wrapping; }

    TextOverflow ScriptText::Internal_GetOverflow(ScriptText* thisPtr) { return thisPtr->GetComponent().Overflow; }

    void ScriptText::Internal_SetOverflow(ScriptText* thisPtr, TextOverflow overflow) { thisPtr->GetComponent().Overflow = overflow; }

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

    bool ScriptText::Internal_GetUseKerning(ScriptText* thisPtr) { return thisPtr->GetComponent().UseKerning; }

    void ScriptText::Internal_SetUseKerning(ScriptText* thisPtr, bool useKerning) { thisPtr->GetComponent().UseKerning = useKerning; }

} // namespace Crowny