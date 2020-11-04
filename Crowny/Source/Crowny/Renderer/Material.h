#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	class Material
	{
	protected:
		std::vector<Ref<Texture>> m_Textures;

		Ref<Shader> m_Shader;
		byte* m_VSUserUniformBuffer;
		uint32_t m_VSUserUniformBufferSize;

		byte* m_PSUserUniformBuffer;
		uint32_t m_PSUserUniformBufferSize;

		const ShaderUniformList* m_VSUserUniforms;
		const ShaderUniformList* m_PSUserUniforms;
		const ShaderResourceList* m_Resources;

	private:
		friend class MaterialInstance;

	public:
		Material(const Ref<Shader>& shader);
		~Material() = default;

		void Bind();
		void Unbind();

		template<typename T>
		void SetUniform(const std::string& name, const T& data)
		{
			byte* buffer;
			ShaderUniformDeclaration* declaration = FindUniformDeclaration(name, &buffer);
			CW_ENGINE_ASSERT(declaration, "Could not find uniform with name " + name + "!");
			memcpy(buffer + declaration->GetOffset(), &data, declaration->GetSize());
		}

		void SetUniformData(const std::string& name, byte* data);
		void SetTexture(const std::string& name, const Ref<Texture>& texture);

		Ref<Shader> GetShader() { return m_Shader; }

//		template<typename T>
//		const T* GetUniform(const std::string& name) const 
//		{
//			return GetUniform<T>(GetUniformDeclaration(name));
//		}

//		template<typename T>
//		const T* GetUniform(const ShaderUniformDeclaration* uniform) const
//		{
//			return (T*)&m_UniformData[uniform->GetOffset()];
//		}
		
	protected:
		void AllocateStorage();
		ShaderUniformDeclaration* FindUniformDeclaration(const std::string& name, byte** outBuffer = nullptr);
		ShaderResourceDeclaration* FindResourceDeclaration(const std::string& name);
	};

	class MaterialInstance
	{
	private:
		Ref<Material> m_Material;
		std::vector<Ref<Texture>> m_Textures;

		byte* m_VSUserUniformBuffer;
		uint32_t m_VSUserUniformBufferSize;

		byte* m_PSUserUniformBuffer;
		uint32_t m_PSUserUniformBufferSize;

		const ShaderUniformList* m_VSUseUniforms;
		const ShaderUniformList* m_PSUseUniforms;
		const ShaderResourceList* m_Resources;

	public:
		MaterialInstance(const Ref<Material>& material);
		const Ref<Material>& GetMaterial() const { return m_Material; }

		void Bind();
		void Unbind();
		void SetUniformData(const std::string& name, byte* data);
		void SetTexture(const std::string& name, const Ref<Texture>& texture);

		template<typename T>
		void SetUniform(const std::string& name, const T& data)
		{
			byte* buf;
			auto* decl = FindUniformDeclaration(name, &buf);
			CW_ENGINE_ASSERT(decl, "");
			memcpy(buf + decl->GetOffset(), &data, decl->GetSize());
		}

//		template<typename T>
//		const T* GetUniform(const std::string& name) const
//		{
//			return GetUniform<T>(GetUniformDeclaration(name));
//		}

	private:
		void AllocateStorage();
		ShaderUniformDeclaration* FindUniformDeclaration(const std::string& name, byte** outBuffer = nullptr);
		ShaderResourceDeclaration* FindResourceDeclaration(const std::string& name);
	};
}
