#pragma once

#include "Crowny/Renderer/Texture.h"

namespace ftgl {
	struct texture_font_t;
	struct texture_atlas_t;
}

namespace Crowny
{
	
	class Font
	{
	public:
		Font(const std::string& filepath, const std::string& name, float size);

		inline const std::string& GetName() const { return m_Name; }

		inline float GetSize() const { return m_Size; }

		inline const std::string& GetFilepath() const { return m_Filepath; }

		inline ftgl::texture_font_t* GetFTGLFont() const { return m_Font; }

		inline ftgl::texture_atlas_t* GetFTGLAtlas() const { return m_Atlas; }

		Ref<Texture2D> GetTexture();

		static float GetWidth(const std::string& font, const std::string& text);
		static float GetHeight(const std::string& font, const std::string& text);
		static float GetWidth(const Ref<Font>& font, const std::string& text);
		static float GetHeight(const Ref<Font>& font, const std::string& text);

	private:
		ftgl::texture_atlas_t* m_Atlas;
		ftgl::texture_font_t* m_Font;
		float m_Size;
		std::string m_Name, m_Filepath;
		Ref<Texture2D> m_Texture;
	};

	class FontManager
	{
	public:
		static void Add(const Ref<Font>& font);
		static Ref<Font> Get(const std::string& name);
		static Ref<Font> Get(const std::string& name, float size);

	private:
		static std::vector<Ref<Font>> s_Fonts;
	};
}