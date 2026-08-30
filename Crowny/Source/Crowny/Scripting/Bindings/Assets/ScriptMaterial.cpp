#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"

namespace Crowny
{
    ScriptMaterial::ScriptMaterial(MonoObject* instance, const AssetHandle<Material>& material) : TScriptAsset(instance, material) {}

    void ScriptMaterial::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_SetFloat", (void*)&Internal_SetFloat);
        MetaData.ScriptClass->AddInternalCall("Internal_SetFloat2", (void*)&Internal_SetFloat2);
        MetaData.ScriptClass->AddInternalCall("Internal_SetInt", (void*)&Internal_SetInt);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColor", (void*)&Internal_SetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVector3", (void*)&Internal_SetVector3);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMatrix", (void*)&Internal_SetMatrix);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTexture", (void*)&Internal_SetTexture);
        MetaData.ScriptClass->AddInternalCall("Internal_HasAlphaModeOverride", (void*)&Internal_HasAlphaModeOverride);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAlphaMode", (void*)&Internal_GetAlphaMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetAlphaMode", (void*)&Internal_SetAlphaMode);
        MetaData.ScriptClass->AddInternalCall("Internal_ClearAlphaModeOverride", (void*)&Internal_ClearAlphaModeOverride);
    }

    void ScriptMaterial::Internal_SetFloat(ScriptMaterial* thisPtr, MonoString* name, float value)
    {
        thisPtr->GetHandle()->SetFloat(MonoUtils::FromMonoString(name), value);
    }

    void ScriptMaterial::Internal_SetFloat2(ScriptMaterial* thisPtr, MonoString* name, glm::vec2* value)
    {
        thisPtr->GetHandle()->SetFloat2(MonoUtils::FromMonoString(name), *value);
    }

    void ScriptMaterial::Internal_SetInt(ScriptMaterial* thisPtr, MonoString* name, int value)
    {
        thisPtr->GetHandle()->SetInt(MonoUtils::FromMonoString(name), value);
    }

    void ScriptMaterial::Internal_SetColor(ScriptMaterial* thisPtr, MonoString* name, glm::vec4* color)
    {
        thisPtr->GetHandle()->SetColor(MonoUtils::FromMonoString(name), *color);
    }

    void ScriptMaterial::Internal_SetVector3(ScriptMaterial* thisPtr, MonoString* name, glm::vec3* value)
    {
        thisPtr->GetHandle()->SetVector3(MonoUtils::FromMonoString(name), *value);
    }

    void ScriptMaterial::Internal_SetMatrix(ScriptMaterial* thisPtr, MonoString* name, glm::mat4* matrix)
    {
        thisPtr->GetHandle()->SetMatrix(MonoUtils::FromMonoString(name), *matrix);
    }

    void ScriptMaterial::Internal_SetTexture(ScriptMaterial* thisPtr, MonoString* name, MonoObject* texture)
    {
        ScriptTexture* scriptTexture = ScriptTexture::ToNative(texture);
        if (scriptTexture != nullptr)
            thisPtr->GetHandle()->SetTexture(MonoUtils::FromMonoString(name), scriptTexture->GetHandle());
        else
            thisPtr->GetHandle()->SetTexture(MonoUtils::FromMonoString(name), AssetHandle<Texture>());
    }

    bool ScriptMaterial::Internal_HasAlphaModeOverride(ScriptMaterial* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetHandle() && thisPtr->GetHandle()->HasAlphaModeOverride();
    }

    int32_t ScriptMaterial::Internal_GetAlphaMode(ScriptMaterial* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetHandle() ? static_cast<int32_t>(thisPtr->GetHandle()->GetAlphaMode())
                                                          : static_cast<int32_t>(AlphaMode::Opaque);
    }

    void ScriptMaterial::Internal_SetAlphaMode(ScriptMaterial* thisPtr, int32_t alphaMode)
    {
        if (thisPtr != nullptr && thisPtr->GetHandle() && alphaMode >= static_cast<int32_t>(AlphaMode::Opaque) &&
            alphaMode <= static_cast<int32_t>(AlphaMode::WeightedOIT))
            thisPtr->GetHandle()->SetAlphaMode(static_cast<AlphaMode>(alphaMode));
    }

    void ScriptMaterial::Internal_ClearAlphaModeOverride(ScriptMaterial* thisPtr)
    {
        if (thisPtr != nullptr && thisPtr->GetHandle())
            thisPtr->GetHandle()->ClearAlphaModeOverride();
    }
} // namespace Crowny
