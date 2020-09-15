#include "cwpch.h"

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
	Material::Material(const Ref<Shader>& shader) : m_Shader(shader)
	{
		AllocateStorage();
		m_Resources = &shader->GetResources();
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

		if (m_VSUserUniformBuffer)
			m_Shader->SetVSUserUniformBuffer(m_VSUserUniformBuffer, m_VSUserUniformBufferSize);
		if (m_PSUserUniformBuffer)
			m_Shader->SetFSUserUniformBuffer(m_PSUserUniformBuffer, m_PSUserUniformBufferSize);

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

	void Material::SetUniformData(const std::string& name, byte* data)
	{
		byte* buf;
		ShaderUniformDeclaration* decl = FindUniformDeclaration(name, &buf);
		memcpy(buf + decl->GetOffset(), data, decl->GetSize());
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

	MaterialInstance::MaterialInstance(const Ref<Material>& material) : m_Material(material)
	{
		m_VSUserUniformBuffer = nullptr;
		m_PSUserUniformBuffer = nullptr;
		AllocateStorage();
		if (m_VSUserUniformBuffer)
			memcpy(m_VSUserUniformBuffer, m_Material->m_VSUserUniformBuffer, m_VSUserUniformBufferSize);
		if (m_PSUserUniformBuffer)
			memcpy(m_PSUserUniformBuffer, m_Material->m_PSUserUniformBuffer, m_PSUserUniformBufferSize);
		
		m_Resources = &m_Material->GetShader()->GetResources();
	}

	void MaterialInstance::AllocateStorage()
	{
		const ShaderUniformBufferDeclaration* vsbuff = m_Material->m_Shader->GetVSUserUniformBuffer();
		if (vsbuff)
		{
			m_VSUserUniformBufferSize = vsbuff->GetSize();
			m_VSUserUniformBuffer = new byte[m_VSUserUniformBufferSize];
			m_VSUseUniforms = &vsbuff->GetUniformDeclarations();
		}

		const ShaderUniformBufferDeclaration* fsbuff = m_Material->m_Shader->GetFSUserUniformBuffer();
		if (fsbuff)
		{
			m_PSUserUniformBufferSize = fsbuff->GetSize();
			m_PSUserUniformBuffer = new byte[m_PSUserUniformBufferSize];
			m_PSUseUniforms = &fsbuff->GetUniformDeclarations();
		}
	}

	void MaterialInstance::Bind()
	{
		m_Material->Bind();

		if (m_VSUserUniformBuffer)
			m_Material->m_Shader->SetVSUserUniformBuffer(m_VSUserUniformBuffer, m_VSUserUniformBufferSize);
		if (m_PSUserUniformBuffer)
			m_Material->m_Shader->SetFSUserUniformBuffer(m_PSUserUniformBuffer, m_PSUserUniformBufferSize);

		for (uint32_t i = 0; i < m_Textures.size(); i++)
		{
			if (m_Textures[i])
				m_Textures[i]->Bind(i);
		}
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
	
	void MaterialInstance::SetUniformData(const std::string& name, byte* data)
	{
		byte* buff;
		ShaderUniformDeclaration* decl = FindUniformDeclaration(name, &buff);
		CW_ENGINE_ASSERT(buff, "");
		memcpy(buff + decl->GetOffset(), data, decl->GetSize());
	}

	void MaterialInstance::SetTexture(const std::string& name, const Ref<Texture2D>& texture)
	{
		ShaderResourceDeclaration* decl = FindResourceDeclaration(name);
		uint32_t slot = decl->GetRegister();
		if (m_Textures.size() <= slot)
			m_Textures.resize(slot + 1);
		m_Textures[slot] = texture;
	}

	ShaderUniformDeclaration* MaterialInstance::FindUniformDeclaration(const std::string& name, byte** outBuffer)
	{
		if (m_VSUseUniforms)
		{
			for (ShaderUniformDeclaration* uniform : *m_VSUseUniforms)
			{
				if (uniform->GetName() == name)
				{
					*outBuffer = m_VSUserUniformBuffer;
					return uniform;
				}
			}
		}

		if (m_PSUseUniforms)
		{
			for (ShaderUniformDeclaration* uniform : *m_PSUseUniforms)
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

	ShaderResourceDeclaration* MaterialInstance::FindResourceDeclaration(const std::string& name)
	{
		for (ShaderResourceDeclaration* resource : *m_Resources)
		{
			if (resource->GetName() == name)
				return resource;
		}

		return nullptr;
	}
}