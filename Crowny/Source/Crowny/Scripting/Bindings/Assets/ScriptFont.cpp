#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"

namespace Crowny
{
    ScriptFont::ScriptFont(MonoObject* instance, const AssetHandle<Font>& font) : TScriptAsset(instance, font) {}

    void ScriptFont::InitRuntimeData() {}

} // namespace Crowny
