#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	class TextureManager
	{
	public:
		TextureManager() = delete;

	private:
		static std::vector<Ref<Texture>> m_Textures;

	public:
		static Ref<Texture> Add(const Ref<Texture>& texture);
		static Ref<Texture> Get(const std::string& name);
		static void Clear();
	};
	
}