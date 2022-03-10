#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace ftgl
{
    struct texture_font_t;
    struct texture_atlas_t;
} // namespace ftgl

namespace Crowny
{

    class Font
    {
    public:
        Font(const Path& filepath, const String& name, float size);
        ~Font();

        const String& GetName() const { return m_Name; }
        float GetSize() const { return m_Size; }
        const Path& GetFilepath() const { return m_Filepath; }

        ftgl::texture_font_t* GetFTGLFont() const { return m_Font; }
        ftgl::texture_atlas_t* GetFTGLAtlas() const { return m_Atlas; }

        const Ref<Texture> GetTexture() const { return m_Texture; };

        static float GetWidth(const String& font, const String& text);
        static float GetHeight(const String& font, const String& text);
        static float GetWidth(const Ref<Font>& font, const String& text);
        static float GetHeight(const Ref<Font>& font, const String& text);

    private:
        ftgl::texture_atlas_t* m_Atlas;
        ftgl::texture_font_t* m_Font;
        float m_Size;
        String m_Name;
        Path m_Filepath;
        Ref<Texture> m_Texture;
    };

    class FontManager
    {
    public:
        static void Add(const Ref<Font>& font);
        static Ref<Font> Get(const String& name);
        static Ref<Font> Get(const String& name, float size);

    private:
        static Vector<Ref<Font>> s_Fonts;
    };
} // namespace Crowny