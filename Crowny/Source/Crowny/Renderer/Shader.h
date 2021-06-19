#pragma once

namespace Crowny
{

	struct BinaryShaderData;

	struct UniformBufferBlock
	{
		std::string Name;
		uint32_t Slot;
		uint32_t Set;
		uint32_t BlockSize;
	};

	struct UniformResource
	{
		std::string Name;
		UniformResourceType Type;
		uint32_t Slot;
		uint32_t Set;
	};

	struct UniformDesc
	{
		std::unordered_map<std::string, UniformBufferBlock> Uniforms;
		
		std::unordered_map<std::string, UniformResource> CombinedSamplers;
		std::unordered_map<std::string, UniformResource> Samplers;
		std::unordered_map<std::string, UniformResource> Textures;
		std::unordered_map<std::string, UniformResource> LoadStoreTextures;
	};

	class Shader
	{
	public:
		virtual void Reload() {}// = 0;

		virtual ~Shader() = default;

		virtual void Bind() const{}// = 0;
		virtual void Unbind() const{}// = 0;
		
		virtual const std::string& GetName() const{}// = 0;
		virtual const std::string& GetFilepath() const{}// = 0 {};
		
		virtual const UniformDesc& GetUniformDesc() const = 0;

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