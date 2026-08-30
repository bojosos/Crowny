#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"

namespace Crowny
{
    ScriptTexture::ScriptTexture(MonoObject* instance, const AssetHandle<Texture>& texture) : TScriptAsset(instance, texture) {}

    void ScriptTexture::InitRuntimeData() {}

} // namespace Crowny
