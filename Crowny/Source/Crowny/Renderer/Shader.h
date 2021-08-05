#pragma once

namespace Crowny
{

	struct BinaryShaderData;

	struct UniformBufferBlockDesc
	{
		std::string Name;
		uint32_t Slot;
		uint32_t Set;
		uint32_t BlockSize;
	};

	struct UniformResourceDesc
	{
		std::string Name;
		UniformResourceType Type;
		uint32_t Slot;
		uint32_t Set;
		GpuBufferFormat ElementType = BF_UNKNOWN;
	};

	struct UniformDesc
	{
		std::unordered_map<std::string, UniformBufferBlockDesc> Uniforms;
		
		std::unordered_map<std::string, UniformResourceDesc> Samplers;
		std::unordered_map<std::string, UniformResourceDesc> Textures;
		std::unordered_map<std::string, UniformResourceDesc> LoadStoreTextures;
	};

	class Shader
	{
	public:
		virtual const Ref<UniformDesc>& GetUniformDesc() const = 0;

		// Creates a shader from a vertex and fragment source
		static Ref<Shader> Create(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);

		// Creates a shader from a file
		static Ref<Shader> Create(const std::string& filepath, ShaderType shaderType = VERTEX_SHADER);

		static Ref<Shader> Create(const BinaryShaderData& shaderData);
	};

	class ShaderLibrary
	{
	public:
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

	private:
		std::string m_Name;
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}        