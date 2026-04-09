#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptTexture : public TScriptAsset<ScriptTexture, Texture>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Texture");
        ScriptTexture(MonoObject* instance, const AssetHandle<Texture>& texture);

    private:
        static uint32_t Internal_GetWidth(ScriptTexture* thisPtr);
        static uint32_t Internal_GetHeight(ScriptTexture* thisPtr);
    };
} // namespace Crowny
