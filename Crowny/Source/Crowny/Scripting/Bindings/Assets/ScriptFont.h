#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    struct ScriptFontCharacterInfo
    {
        UUID SourceFont;
        uint32_t RequestedCodePoint = 0;
        uint32_t ResolvedCodePoint = 0;
        int32_t GlyphIndex = -1;
        uint32_t Reserved = 0;
        double Advance = 0.0;
        double PlaneLeft = 0.0;
        double PlaneBottom = 0.0;
        double PlaneRight = 0.0;
        double PlaneTop = 0.0;
        double AtlasLeft = 0.0;
        double AtlasBottom = 0.0;
        double AtlasRight = 0.0;
        double AtlasTop = 0.0;
        bool Whitespace = false;
        bool Valid = false;
    };

    class ScriptFont : public TScriptAsset<ScriptFont, Font>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Font");
        ScriptFont(MonoObject* instance, const AssetHandle<Font>& font);

    private:
    };

} // namespace Crowny
