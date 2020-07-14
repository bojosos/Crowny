#include "cwpch.h"

#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
	PBRMaterial::PBRMaterial(const Ref<Shader>& shader) : Material(shader)
	{
		SetUniform("u_UsingNormalMap", 0.0f);
		SetUniform("u_UsingAlbedoMap", 0.0f);
		SetUniform("u_AlbedoColor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

		SetUniform("u_SpecularColor", glm::vec3(1.0f, 1.0f, 1.0f));
		SetUniform("u_UsingSpecularMap", 0.0f);

		SetUniform("u_GlossColor", 0.8f);
		SetUniform("u_UsingGlossMap", 0.0f);

		SetUniform("u_UsingNormalMap", 0.0f);
	}

	void PBRMaterial::SetEnvironmentMap()
	{
		//SetTexture("u_EnvironmentMap", texture);
	}

	void PBRMaterial::SetAlbedo(const glm::vec4& color)
	{
		SetUniform("u_AlbedoColor", color);
		SetUniform("u_UsingAlbedoMap", 0.0f);
	}

	void PBRMaterial::SetSpecular(const glm::vec3& color)
	{
		SetUniform("u_SpecularColor", color);
		SetUniform("u_UsingSpecularMap", 0.0f);
	}

	void PBRMaterial::SetGloss(float value)
	{
		SetUniform("u_GlossColor", value);
		SetUniform("u_UsingGlossMap", 0.0f);
	}

	void PBRMaterial::UsingNormalMap(bool value)
	{
		SetUniform("u_UsingNormalMap", value ? 1.0f : 0.0f);
	}

	void PBRMaterial::SetAlbedoMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_AlbedoMap", texture);
		SetUniform("u_UsingAlbedoMap", 1.0f);
	}

	void PBRMaterial::SetSpecularMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_SpecularMap", texture);
		SetUniform("u_UsingSpecularMap", 1.0f);
	}

	void PBRMaterial::SetNormalMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_NormalMap", texture);
		SetUniform("u_UsingNormalMap", 1.0f);
	}

	void PBRMaterial::SetGlossMap(const Ref<Texture2D>& texture)
	{
		SetTexture("u_GlossMap", texture);
		SetUniform("u_UsingGlossMap", 1.0f);
	}

	Ref<Texture2D> PBRMaterial::GetAlbedoMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_AlbedoMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}

	Ref<Texture2D> PBRMaterial::GetSpecularMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_SpecularMap");
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

	Ref<Texture2D> PBRMaterial::GetGlossMap()
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration("u_GlossMap");
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		return m_Textures.size() > slot ? m_Textures[slot] : nullptr;
	}


}