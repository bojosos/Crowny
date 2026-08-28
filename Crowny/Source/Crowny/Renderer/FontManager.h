#pragma once

#include "Crowny/Renderer/Font.h"

namespace Crowny
{
    class FontManager final
    {
    public:
        FontManager() = delete;

        static bool Register(StringView name, const AssetHandle<Font>& font);
        static bool Remove(StringView name);
        static bool Contains(StringView name);

        // Find only returns an exact named match. Get falls back to the default font.
        static AssetHandle<Font> Find(StringView name);
        static AssetHandle<Font> Get(StringView name);

        static AssetHandle<Font> GetDefaultFont();
        static void SetDefaultFont(const AssetHandle<Font>& font);
        static void Clear();

    private:
        static Mutex s_Mutex;
        static UnorderedMap<String, AssetHandle<Font>> s_Fonts;
    };
} // namespace Crowny
