#pragma once
#include <freetype-gl.h>
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	class Font
	{
	public:
		Font(const std::string& filepath, const std::string& name, float size);

		inline const std::string& GetName() const
		{
			return m_Name;
		}

		inline const float GetSize() const
		{
			return m_Size;
		}

		inline const uint32_t GetId() const
		{
			return m_Atlas->id;
		}

		inline const std::string& GetFilepath() const
		{
			return m_Filepath;
		}

		inline ftgl::texture_font_t* GetFTGLFont() const
		{
			return m_Font;
		}

		inline ftgl::texture_atlas_t* GetFTGLAtlas() const
		{
			return m_Atlas;
		}

		Ref<Texture2D> GetTexture();

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

		static float GetWidth(const std::string& font, const std::string& text);
		static float GetHeight(const std::string& font, const std::string& text);
		static float GetWidth(const Ref<Font>& font, const std::string& text);
		static float GetHeight(const Ref<Font>& font, const std::string& text);

	private:
		static std::vector<Ref<Font>> s_Fonts;
		FontManager();
	};
}