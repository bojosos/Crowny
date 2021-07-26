#include "cwpch.h"

#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
	PBRMaterial::PBRMaterial(const Ref<Shader>& shader) : Material(shader)
	{
		SetAlbedo(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		SetMetalness(0.8f);
		SetRoughness(0.2f);
	}

	void PBRMaterial::SetEnvironmentMap()
	{
		//SetTexture("u_EnvironmentMap", texture);
	}

	void PBRMaterial::SetAlbedo(const glm::vec4& color)
	{
		//SetUniform("u_Albedo", color);
	}

	void PBRMaterial::SetMetalness(float value)
	{
		//SetUniform("u_Metalness", value);
	}

	void PBRMaterial::SetRoughness(float value)
	{
		//SetUniform("u_Roughness", value);
	}

	void PBRMaterial::SetAlbedoMap(const Ref<Texture>& texture)
	{
		//SetTexture("u_AlbedoMap", texture);
	}

	void PBRMaterial::SetMetalnessMap(const Ref<Texture>& texture)
	{
		//SetTexture("u_MetalnessMap", texture);
	}

	void PBRMaterial::SetNormalMap(const Ref<Texture>& texture)
	{
		//SetTexture("u_NormalMap", texture);
	}

	void PBRMaterial::SetRoughnessMap(const Ref<Texture>& texture)
	{
		//SetTexture("u_RoughnessMap", texture);
	}

	void PBRMaterial::SetAoMap(const Ref<Texture>& texture)
	{
		//SetTexture("u_AoMap", texture);
	}

	Ref<Texture> PBRMaterial::GetAlbedoMap()
	{/*
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AlbedoMap");
		CW_ENGINE_ASSERT(declaration);
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;*/
	}

	Ref<Texture> PBRMaterial::GetMetalnessMap()
	{/*
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_MetalnessMap");
		CW_ENGINE_ASSERT(declaration);
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;*/
	}

	Ref<Texture> PBRMaterial::GetNormalMap()
	{/*
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_NormalMap");
		CW_ENGINE_ASSERT(declaration);
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;*/
	}

	Ref<Texture> PBRMaterial::GetRoughnessMap()
	{/*
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_RoughnessMap");
		CW_ENGINE_ASSERT(declaration);
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;*/
	}

	Ref<Texture> PBRMaterial::GetAoMap()
	{/*
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AoMap");
		CW_ENGINE_ASSERT(declaration);
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;*/
	}
}