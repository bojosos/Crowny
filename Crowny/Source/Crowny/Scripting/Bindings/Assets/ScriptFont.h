#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

#include "Crowny/Scripting/Mono/MonoArray.h"

namespace Crowny
{

    class ScriptFont : public TScriptAsset<ScriptFont, Font>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Font");
        ScriptFont(MonoObject* instance, const AssetHandle<Font>& font);

    private:
        static ScriptArray Internal_GetCharacterInfos(ScriptFont* thisptr) { return ScriptArray::Create<int>(1); }
        static bool Internal_GetCharacterInfo(char c, CharacterInfo* characterInfo, int size, TextFontStyle style)
        {
            return false;
        }
    };

} // namespace Crowny