#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

#include <cstddef>
#include <limits>

namespace Crowny
{
    namespace
    {
        UUID GetSourceFontUuid(const AssetHandle<Font>& font, const Font* source)
        {
            if (source == nullptr)
                return {};
            if (font.Get() == source)
                return font.GetUUID();
            if (!font)
                return {};
            return font->FindFallbackFontUUID(source);
        }

        ScriptFontCharacterInfo ToScriptCharacterInfo(const AssetHandle<Font>& font, const CharacterInfo& source)
        {
            ScriptFontCharacterInfo result;
            result.SourceFont = GetSourceFontUuid(font, source.SourceFont);
            result.RequestedCodePoint = static_cast<uint32_t>(source.RequestedCodePoint);
            result.ResolvedCodePoint = static_cast<uint32_t>(source.ResolvedCodePoint);
            result.GlyphIndex = source.GlyphIndex;
            result.Advance = source.Advance;
            result.PlaneLeft = source.PlaneLeft;
            result.PlaneBottom = source.PlaneBottom;
            result.PlaneRight = source.PlaneRight;
            result.PlaneTop = source.PlaneTop;
            result.AtlasLeft = source.AtlasLeft;
            result.AtlasBottom = source.AtlasBottom;
            result.AtlasRight = source.AtlasRight;
            result.AtlasTop = source.AtlasTop;
            result.Whitespace = source.Whitespace;
            result.Valid = source.Valid;
            return result;
        }
    } // namespace

    static_assert(sizeof(ScriptFontCharacterInfo) == 112, "Managed font character layout changed.");
    static_assert(offsetof(ScriptFontCharacterInfo, Advance) == 32, "Managed font character alignment changed.");
    static_assert(offsetof(ScriptFontCharacterInfo, Valid) == 105, "Managed font character flag layout changed.");

    ScriptFont::ScriptFont(MonoObject* instance, const AssetHandle<Font>& font) : TScriptAsset(instance, font) {}

    void ScriptFont::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetIsValid", (void*)&Internal_GetIsValid);
        MetaData.ScriptClass->AddInternalCall("Internal_GetGlyphCount", (void*)&Internal_GetGlyphCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetTabWidth", (void*)&Internal_GetTabWidth);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAtlasWidth", (void*)&Internal_GetAtlasWidth);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAtlasHeight", (void*)&Internal_GetAtlasHeight);
        MetaData.ScriptClass->AddInternalCall("Internal_GetAtlasPixelRange", (void*)&Internal_GetAtlasPixelRange);
        MetaData.ScriptClass->AddInternalCall("Internal_HasGlyph", (void*)&Internal_HasGlyph);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCharacterInfo", (void*)&Internal_GetCharacterInfo);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFallbackCount", (void*)&Internal_GetFallbackCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFallback", (void*)&Internal_GetFallback);
        MetaData.ScriptClass->AddInternalCall("Internal_AddFallback", (void*)&Internal_AddFallback);
        MetaData.ScriptClass->AddInternalCall("Internal_ClearFallbacks", (void*)&Internal_ClearFallbacks);
    }

    bool ScriptFont::Internal_GetIsValid(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() && thisptr->GetHandle()->IsValid();
    }

    uint32_t ScriptFont::Internal_GetGlyphCount(ScriptFont* thisptr)
    {
        if (thisptr == nullptr || !thisptr->GetHandle())
            return 0;
        return static_cast<uint32_t>(std::min(thisptr->GetHandle()->GetGlyphCount(), static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
    }

    uint32_t ScriptFont::Internal_GetTabWidth(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() ? thisptr->GetHandle()->GetTabWidth() : 0;
    }

    uint32_t ScriptFont::Internal_GetAtlasWidth(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() ? thisptr->GetHandle()->GetAtlasWidth() : 0;
    }

    uint32_t ScriptFont::Internal_GetAtlasHeight(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() ? thisptr->GetHandle()->GetAtlasHeight() : 0;
    }

    float ScriptFont::Internal_GetAtlasPixelRange(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() ? thisptr->GetHandle()->GetAtlasPixelRange() : 0.0f;
    }

    bool ScriptFont::Internal_HasGlyph(ScriptFont* thisptr, uint32_t codePoint)
    {
        return thisptr != nullptr && thisptr->GetHandle() && thisptr->GetHandle()->HasGlyph(static_cast<char32_t>(codePoint));
    }

    bool ScriptFont::Internal_GetCharacterInfo(ScriptFont* thisptr, uint32_t codePoint, bool useFallbacks, ScriptFontCharacterInfo* characterInfo)
    {
        if (characterInfo == nullptr)
            return false;
        *characterInfo = {};
        if (thisptr == nullptr || !thisptr->GetHandle())
            return false;
        const CharacterInfo nativeInfo = thisptr->GetHandle()->GetCharacterInfo(static_cast<char32_t>(codePoint), useFallbacks);
        *characterInfo = ToScriptCharacterInfo(thisptr->GetHandle(), nativeInfo);
        return nativeInfo.Valid;
    }

    uint32_t ScriptFont::Internal_GetFallbackCount(ScriptFont* thisptr)
    {
        return thisptr != nullptr && thisptr->GetHandle() ? static_cast<uint32_t>(thisptr->GetHandle()->GetFallbackFonts().size()) : 0;
    }

    MonoObject* ScriptFont::Internal_GetFallback(ScriptFont* thisptr, uint32_t index)
    {
        if (thisptr == nullptr || !thisptr->GetHandle() || !ScriptAssetManager::IsStartedUp())
            return nullptr;
        const Vector<AssetHandle<Font>>& fallbacks = thisptr->GetHandle()->GetFallbackFonts();
        if (index >= fallbacks.size())
            return nullptr;
        ScriptAssetBase* asset = ScriptAssetManager::Get().GetScriptAsset(fallbacks[index], true);
        return asset != nullptr ? asset->GetManagedInstance() : nullptr;
    }

    bool ScriptFont::Internal_AddFallback(ScriptFont* thisptr, MonoObject* fallback)
    {
        if (thisptr == nullptr || !thisptr->GetHandle())
            return false;
        ScriptFont* nativeFallback = ScriptFont::ToNative(fallback);
        return nativeFallback != nullptr && thisptr->GetHandle()->AddFallbackFont(nativeFallback->GetHandle());
    }

    void ScriptFont::Internal_ClearFallbacks(ScriptFont* thisptr)
    {
        if (thisptr != nullptr && thisptr->GetHandle())
            thisptr->GetHandle()->ClearFallbackFonts();
    }
} // namespace Crowny
