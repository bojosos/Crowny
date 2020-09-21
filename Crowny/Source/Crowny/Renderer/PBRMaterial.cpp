#include "cwpch.h"

#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
	PBRMaterial::PBRMaterial(const Ref<Shader>& shader) : Material(shader)
	{
		SetUniform("u_Albedo", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		SetUniform("u_Metalness", 0.8f);
		SetUniform("u_Roughness", 0.2f);
	}

	void PBRMaterial::SetEnvironmentMap()
	{
		//SetTexture("u_EnvironmentMap", texture);
	}

	void PBRMaterial::SetAlbedo(const glm::vec4& color)
	{
		SetUniform("u_Albedo", color);
	}

	void PBRMaterial::SetMetalness(float value)
	{
		SetUniform("u_Metalness", value);
	}

	void PBRMaterial::SetRoughness(float value)
	{
		SetUniform("u_Roughness", value);
	}

	void PBRMaterial::SetAlbedoMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_AlbedoMap", texture);
	}

	void PBRMaterial::SetMetalnessMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_MetalnessMap", texture);
	}

	void PBRMaterial::SetNormalMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_NormalMap", texture);
	}

	void PBRMaterial::SetRoughnessMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_RoughnessMap", texture);
	}

	void PBRMaterial::SetAoMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_AoMap", texture);
	}

	Ref<Texture2D> PBRMaterial::GetAlbedoMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AlbedoMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetMetalnessMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_MetalnessMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetNormalMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_NormalMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetRoughnessMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_RoughnessMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetAoMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AoMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? std::dynamic_pointer_cast<Texture2D>(m_Textures[slot]) : nullptr;
	}
}