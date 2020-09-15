#include "cwpch.h"

#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
	PBRMaterial::PBRMaterial(const Ref<Shader>& shader) : Material(shader)
	{
		//SetUniform("u_UsingNormalMap", 0.0f);
		//SetUniform("u_UsingAlbedoMap", 0.0f);
		//SetUniform("u_AlbedoColor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

		//SetUniform("u_Metalness", 0.8f);
		//SetUniform("u_UsingMetalness", 0.0f);

		//SetUniform("u_Roughness", 0.2f);
		//SetUniform("u_UsingRoughnessMap", 0.0f);

		//SetUniform("u_UsingNormalMap", 0.0f);
	}

	void PBRMaterial::SetEnvironmentMap()
	{
		//SetTexture("u_EnvironmentMap", texture);
	}

	void PBRMaterial::SetAlbedo(const glm::vec4& color)
	{
		SetUniform("u_AlbedoColor", color);
		//SetUniform("u_UsingAlbedoMap", 0.0f);
	}

	void PBRMaterial::SetMetalness(float value)
	{
		SetUniform("u_Metalness", value);
		//SetUniform("u_UsingMetalnessMap", 0.0f);
	}

	void PBRMaterial::SetRougness(float value)
	{
		SetUniform("u_Roughness", value);
		//SetUniform("u_UsingRougnessMap", 0.0f);
	}

	void PBRMaterial::UsingNormalMap(bool value)
	{
		SetUniform("u_UsingNormalMap", value ? 1.0f : 0.0f);
	}

	void PBRMaterial::SetAlbedoMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_AlbedoMap", texture);
		//SetUniform("u_UsingAlbedoMap", 1.0f);
	}

	void PBRMaterial::SetMetalnessMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_MetalnessMap", texture);
		//SetUniform("u_UsingSpecularMap", 1.0f);
	}

	void PBRMaterial::SetNormalMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_NormalMap", texture);
		//SetUniform("u_UsingNormalMap", 1.0f);
	}

	void PBRMaterial::SetRougnessMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_RoughnessMap", texture);
		//SetUniform("u_UsingRougnessMap", 1.0f);
	}

	void PBRMaterial::SetAoMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_AoMap", texture);
		//SetUniform("u_UsingAoMap", 1.0f);
	}

	Ref<Texture2D> PBRMaterial::GetAlbedoMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AlbedoMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetMetalnessMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_MetalnessMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetNormalMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_NormalMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetRoughnessMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_RoughnessMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetAoMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AoMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}
}