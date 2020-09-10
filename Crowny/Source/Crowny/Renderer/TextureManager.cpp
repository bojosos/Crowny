#include "cwpch.h"

#include "Crowny/Renderer/TextureManager.h"

namespace Crowny
{
	std::vector<Ref<Texture>> TextureManager::m_Textures;

	Ref<Texture> TextureManager::Add(const Ref<Texture>& texture)
	{
		m_Textures.push_back(texture);
		return texture;
	}

	Ref<Texture> TextureManager::Get(const std::string& name)
	{
		for (auto& texture : m_Textures)
		{
			if (texture->GetName() == name)
				return texture;
		}
	}

	void TextureManager::Clear()
	{
		m_Textures.clear();
	}

	Ref<Texture2D> Textures::Unassigned = nullptr;

	void Textures::LoadDefault()
	{
		Unassigned = Texture2D::Create("/Textures/Unassigned.png");
	}
}