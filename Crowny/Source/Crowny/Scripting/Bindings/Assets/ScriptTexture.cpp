#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"

namespace Crowny
{
    ScriptTexture::ScriptTexture(MonoObject* instance, const AssetHandle<Texture>& texture) : TScriptAsset(instance, texture) {}

    void ScriptTexture::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetWidth", (void*)&Internal_GetWidth);
        MetaData.ScriptClass->AddInternalCall("Internal_GetHeight", (void*)&Internal_GetHeight);
    }

    uint32_t ScriptTexture::Internal_GetWidth(ScriptTexture* thisPtr) { return thisPtr->GetHandle()->GetWidth(); }

    uint32_t ScriptTexture::Internal_GetHeight(ScriptTexture* thisPtr) { return thisPtr->GetHandle()->GetHeight(); }
} // namespace Crowny
