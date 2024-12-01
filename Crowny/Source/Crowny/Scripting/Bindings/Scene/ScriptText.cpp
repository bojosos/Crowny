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
        MetaData.ScriptClass->AddInternalCall("Internal_SetText", (void*)&Internal_GetText);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFont", (void*)&Internal_GetFont);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFont", (void*)&Internal_SetFont);
        MetaData.ScriptClass->AddInternalCall("Internal_GetColor", (void*)&Internal_GetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColor", (void*)&Internal_SetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOutlineColor", (void*)&Internal_GetOutlineColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOutlineColor", (void*)&Internal_SetOutlineColor);
    }

    MonoString* ScriptText::Internal_GetText(ScriptText* thisPtr)
    {
        return MonoUtils::ToMonoString(thisPtr->GetComponent().Text);
    }

    void ScriptText::Internal_SetText(ScriptText* thisPtr, MonoString* text)
    {
        thisPtr->GetComponent().Text = MonoUtils::FromMonoString(text);
    }

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

    void ScriptText::Internal_GetColor(ScriptText* thisPtr, glm::vec4* color)
    {
        *color = thisPtr->GetComponent().Color;
    }

    void ScriptText::Internal_SetColor(ScriptText* thisPtr, glm::vec4* color)
    {
        thisPtr->GetComponent().Color = *color;
    }

    void ScriptText::Internal_GetOutlineColor(ScriptText* thisPtr, glm::vec4* color)
    {
        *color = thisPtr->GetComponent().Color;
    }

    void ScriptText::Internal_SetOutlineColor(ScriptText* thisPtr, glm::vec4* color)
    {
        thisPtr->GetComponent().Color = *color;
    }

} // namespace Crowny