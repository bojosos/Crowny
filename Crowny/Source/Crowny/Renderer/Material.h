#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	class Material
	{
	protected:
		std::vector<Ref<Texture2D>> m_Textures;

		Ref<Shader> m_Shader;
		uint8_t* m_VSUserUniformBuffer;
		uint32_t m_VSUserUniformBufferSize;

		uint8_t* m_PSUserUniformBuffer;
		uint32_t m_PSUserUniformBufferSize;

		const ShaderUniformList* m_VSUserUniforms;
		const ShaderUniformList* m_PSUserUniforms;
		const ShaderResourceList* m_Resources;

	public:
		Material(const Ref<Shader>& shader);

		void Bind();
		void Unbind();

		template<typename T>
		void SetUniform(const std::string& name, const T& data)
		{
			uint8_t* buffer;
			ShaderUniformDeclaration* declaration = FindUniformDeclaration(name, &buffer);
			if (!declaration)
			{
				CW_ENGINE_ERROR("Could not find uniform with name '", name, "'!");
				return;
			}
			memcpy(buffer + declaration->GetOffset(), &data, declaration->GetSize());
		}

		void SetTexture(const std::string& name, const Ref<Texture2D>& texture);
	protected:
		void AllocateStorage();
		ShaderUniformDeclaration* FindUniformDeclaration(const std::string& name, uint8_t** outBuffer = nullptr);
		ShaderResourceDeclaration* FindResourceDeclaration(const std::string& name);
	};

	class MaterialInstance
	{
	private:
		Ref<Material> m_Material;
		std::vector<Ref<Texture2D>> m_Textures;

	public:
		MaterialInstance(const Ref<Material>& material);
		const Ref<Material>& GetMaterial() const { return m_Material; }

		void Bind();
		void Unbind();
	};
}