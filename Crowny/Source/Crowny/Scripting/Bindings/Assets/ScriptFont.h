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
        static bool Internal_GetIsValid(ScriptFont* thisptr);
        static uint32_t Internal_GetGlyphCount(ScriptFont* thisptr);
        static uint32_t Internal_GetTabWidth(ScriptFont* thisptr);
        static uint32_t Internal_GetAtlasWidth(ScriptFont* thisptr);
        static uint32_t Internal_GetAtlasHeight(ScriptFont* thisptr);
        static float Internal_GetAtlasPixelRange(ScriptFont* thisptr);
        static bool Internal_HasGlyph(ScriptFont* thisptr, uint32_t codePoint);
        static bool Internal_GetCharacterInfo(ScriptFont* thisptr, uint32_t codePoint, bool useFallbacks,
                                              ScriptFontCharacterInfo* characterInfo);
        static uint32_t Internal_GetFallbackCount(ScriptFont* thisptr);
        static MonoObject* Internal_GetFallback(ScriptFont* thisptr, uint32_t index);
        static bool Internal_AddFallback(ScriptFont* thisptr, MonoObject* fallback);
        static void Internal_ClearFallbacks(ScriptFont* thisptr);
    };

} // namespace Crowny
