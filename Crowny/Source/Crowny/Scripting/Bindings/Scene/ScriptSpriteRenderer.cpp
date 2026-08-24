#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSpriteRenderer.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{
    ScriptSpriteRenderer::ScriptSpriteRenderer(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptSpriteRenderer::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetTexture", (void*)&Internal_GetTexture);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTexture", (void*)&Internal_SetTexture);
        MetaData.ScriptClass->AddInternalCall("Internal_GetColor", (void*)&Internal_GetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetColor", (void*)&Internal_SetColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSortingLayer", (void*)&Internal_GetSortingLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSortingLayer", (void*)&Internal_SetSortingLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOrderInLayer", (void*)&Internal_GetOrderInLayer);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOrderInLayer", (void*)&Internal_SetOrderInLayer);
    }

    MonoObject* ScriptSpriteRenderer::Internal_GetTexture(ScriptSpriteRenderer* thisPtr)
    {
        ScriptAssetBase* asset = ScriptAssetManager::Get().GetScriptAsset(thisPtr->GetComponent().Texture, true);
        return asset != nullptr ? asset->GetManagedInstance() : nullptr;
    }

    void ScriptSpriteRenderer::Internal_SetTexture(ScriptSpriteRenderer* thisPtr, MonoObject* texture)
    {
        ScriptTexture* nativeTexture = ScriptTexture::ToNative(texture);
        thisPtr->GetComponent().Texture = nativeTexture != nullptr ? nativeTexture->GetHandle() : AssetHandle<Texture>();
    }

    void ScriptSpriteRenderer::Internal_GetColor(ScriptSpriteRenderer* thisPtr, glm::vec4* value)
    {
        *value = thisPtr->GetComponent().Color;
    }

    void ScriptSpriteRenderer::Internal_SetColor(ScriptSpriteRenderer* thisPtr, glm::vec4* value)
    {
        thisPtr->GetComponent().Color = *value;
    }

    int32_t ScriptSpriteRenderer::Internal_GetSortingLayer(ScriptSpriteRenderer* thisPtr)
    {
        return thisPtr->GetComponent().SortingLayer;
    }

    void ScriptSpriteRenderer::Internal_SetSortingLayer(ScriptSpriteRenderer* thisPtr, int32_t value)
    {
        thisPtr->GetComponent().SortingLayer = value;
    }

    int32_t ScriptSpriteRenderer::Internal_GetOrderInLayer(ScriptSpriteRenderer* thisPtr)
    {
        return thisPtr->GetComponent().OrderInLayer;
    }

    void ScriptSpriteRenderer::Internal_SetOrderInLayer(ScriptSpriteRenderer* thisPtr, int32_t value)
    {
        thisPtr->GetComponent().OrderInLayer = value;
    }
} // namespace Crowny
