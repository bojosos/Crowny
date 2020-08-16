#include "cwpch.h"

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
	Material::Material(const Ref<Shader>& shader) : m_Shader(shader)
	{

	}

	MaterialInstance::MaterialInstance(const Ref<Material>& material) : m_Material(material)
	{

	}

	void Material::AllocateStorage()
	{
		m_VSUserUniformBuffer = nullptr;
		m_VSUserUniformBufferSize = 0;

		m_PSUserUniformBuffer = nullptr;
		m_PSUserUniformBufferSize = 0;

		m_VSUserUniforms = nullptr;
		m_PSUserUniforms = nullptr;

		const ShaderUniformBufferDeclaration* vsBuffer = m_Shader->GetVSUserUniformBuffer();
		if (vsBuffer)
		{
			m_VSUserUniformBufferSize = vsBuffer->GetSize();
			m_VSUserUniformBuffer = new byte[m_VSUserUniformBufferSize];
			memset(m_VSUserUniformBuffer, 0, m_VSUserUniformBufferSize);
			m_VSUserUniforms = &vsBuffer->GetUniformDeclarations();
		}

		const ShaderUniformBufferDeclaration* psBuffer = m_Shader->GetFSUserUniformBuffer();
		if (psBuffer)
		{
			m_PSUserUniformBufferSize = psBuffer->GetSize();
			m_PSUserUniformBuffer = new byte[m_PSUserUniformBufferSize];
			memset(m_PSUserUniformBuffer, 0, m_PSUserUniformBufferSize);
			m_PSUserUniforms = &psBuffer->GetUniformDeclarations();
		}
	}

	void Material::Bind()
	{
		m_Shader->Bind();

		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
			if (m_Textures[i])
				m_Textures[i]->Bind(i);
		}
	}

	void Material::Unbind()
	{
		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
			if (m_Textures[i])
				m_Textures[i]->Unbind(i);
		}
	}

	void MaterialInstance::Bind()
	{
		m_Material->Bind();

		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
			if (m_Textures[i])
				m_Textures[i]->Bind(i);
		}
	}

	void Material::SetTexture(const std::string& name, const Ref<Texture2D>& texture)
	{
		ShaderResourceDeclaration* declaration = FindResourceDeclaration(name);
		CW_ENGINE_ASSERT(declaration, "");
		uint32_t slot = declaration->GetRegister();
		if (m_Textures.size() <= slot)
			m_Textures.resize(slot + 1);
		m_Textures[slot] = texture;
	}

	ShaderUniformDeclaration* Material::FindUniformDeclaration(const std::string& name, byte** outBuffer)
	{
		if (m_VSUserUniforms)
		{
			for (ShaderUniformDeclaration* uniform : *m_VSUserUniforms)
			{
				if (uniform->GetName() == name)
				{
					*outBuffer = m_VSUserUniformBuffer;
					return uniform;
				}
			}
		}
		if (m_PSUserUniforms)
		{
			for (ShaderUniformDeclaration* uniform : *m_PSUserUniforms)
			{
				if (uniform->GetName() == name)
				{
					*outBuffer = m_PSUserUniformBuffer;
					return uniform;
				}
			}
		}
		return nullptr;
	}

	ShaderResourceDeclaration* Material::FindResourceDeclaration(const std::string& name)
	{
		for (ShaderResourceDeclaration* resource : *m_Resources)
		{
			if (resource->GetName() == name)
				return resource;
		}
		return nullptr;
	}

	void MaterialInstance::Unbind()
	{
		m_Material->Unbind();
		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
			if (m_Textures[i])
				m_Textures[i]->Unbind(i);
		}
	}
}