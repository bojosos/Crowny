#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"

namespace Crowny
{
    ScriptFont::ScriptFont(MonoObject* instance, const AssetHandle<Font>& font) : TScriptAsset(instance, font) {}

    void ScriptFont::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetCharacterInfos", (void*)&Internal_GetCharacterInfos);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCharacterInfo", (void*)&Internal_GetCharacterInfo);
    }
} // namespace Crowny