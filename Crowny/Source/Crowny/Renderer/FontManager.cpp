#include "cwpch.h"

#include "Crowny/Renderer/FontManager.h"

namespace Crowny
{
    Mutex FontManager::s_Mutex;
    UnorderedMap<String, AssetHandle<Font>> FontManager::s_Fonts;

    bool FontManager::Register(StringView name, const AssetHandle<Font>& font)
    {
        if (name.empty() || !font)
            return false;

        const ScopedLock lock(s_Mutex);
        s_Fonts[String(name)] = font;
        return true;
    }

    bool FontManager::Remove(StringView name)
    {
        if (name.empty())
            return false;
        const ScopedLock lock(s_Mutex);
        return s_Fonts.erase(String(name)) != 0;
    }

    bool FontManager::Contains(StringView name)
    {
        if (name.empty())
            return false;
        const ScopedLock lock(s_Mutex);
        return s_Fonts.find(String(name)) != s_Fonts.end();
    }

    AssetHandle<Font> FontManager::Find(StringView name)
    {
        if (name.empty())
            return {};
        const ScopedLock lock(s_Mutex);
        const auto font = s_Fonts.find(String(name));
        return font != s_Fonts.end() ? font->second : AssetHandle<Font>{};
    }

    AssetHandle<Font> FontManager::Get(StringView name)
    {
        const ScopedLock lock(s_Mutex);
        if (!name.empty())
        {
            const auto font = s_Fonts.find(String(name));
            if (font != s_Fonts.end())
                return font->second;
        }
        return Font::GetDefaultFont();
    }

    AssetHandle<Font> FontManager::GetDefaultFont()
    {
        const ScopedLock lock(s_Mutex);
        return Font::GetDefaultFont();
    }

    void FontManager::SetDefaultFont(const AssetHandle<Font>& font)
    {
        const ScopedLock lock(s_Mutex);
        Font::SetDefaultFont(font);
    }

    void FontManager::Clear()
    {
        const ScopedLock lock(s_Mutex);
        s_Fonts.clear();
        Font::SetDefaultFont({});
    }
} // namespace Crowny
