#pragma once

namespace Crowny
{

	class ShaderResourceDeclaration
	{
	public:
		virtual const std::string& GetName() const = 0;
		virtual uint32_t GetRegister() const = 0;
		virtual uint32_t GetCount() const = 0;
	};

	using ShaderResourceList = std::vector<ShaderResourceDeclaration*>;

	class ShaderUniformDeclaration
	{
		friend class ShaderStruct;
	public:
		virtual const std::string& GetName() const = 0;
		virtual uint32_t GetOffset() const = 0;
		virtual uint32_t GetCount() const = 0;
		virtual uint32_t GetSize() const = 0;
		virtual uint32_t GetAbsoluteOffset() const = 0;

	protected:
		virtual void SetOffset(uint32_t offset) = 0;
	};

	using ShaderUniformList = std::vector<ShaderUniformDeclaration*>;

	class ShaderUniformBufferDeclaration
	{
	public:
		virtual const std::string& GetName() const = 0;
		virtual uint32_t GetRegister() const = 0;
		virtual uint32_t GetShaderType() const = 0;
		virtual uint32_t GetSize() const = 0;

		virtual const ShaderUniformList& GetUniformDeclarations() const = 0;

		virtual ShaderUniformDeclaration* FindUniform(const std::string& name) = 0;
	};

	using ShaderUniformBufferList = std::vector<ShaderUniformBufferDeclaration*>;

	class ShaderStruct
	{
	private:
		std::string m_Name;
		std::vector<ShaderUniformDeclaration*> m_Fields;
		uint32_t m_Size;
		uint32_t m_Offset;
	public:
		ShaderStruct(const std::string& name) : m_Name(name), m_Size(0), m_Offset(0)
		{

		}

		void AddField(ShaderUniformDeclaration* field)
		{
			m_Size += field->GetSize();
			uint32_t offset = 0;
			if (m_Fields.size())
			{
				ShaderUniformDeclaration* prev = m_Fields.back();
				offset = prev->GetOffset() + prev->GetSize();
			}
			field->SetOffset(offset);
			m_Fields.push_back(field);
		}

		void SetOffset(uint32_t offset) { m_Offset = offset; }

		const std::string& GetName() const { return m_Name; }
		uint32_t GetSize() const { return m_Size; };
		uint32_t GetOffset() const { return m_Offset; }
		const std::vector<ShaderUniformDeclaration*>& GetFields() const { return m_Fields; }
	};

	using ShaderStructList = std::vector<ShaderStruct*>;

	class Shader
	{
	public:
		virtual void Reload() {}// = 0;

		virtual ~Shader() = default;

		virtual void Bind() const{}// = 0;
		virtual void Unbind() const{}// = 0;
		
		virtual void SetFSUserUniformBuffer(byte* data, uint32_t size){};// = 0;
		virtual void SetVSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot){};// = 0;
		virtual void SetFSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot){};// = 0;
		virtual void SetVSUserUniformBuffer(byte* data, uint32_t size){};// = 0;

		virtual const ShaderUniformBufferList& GetVSSystemUniforms() const{};// = 0;
		virtual const ShaderUniformBufferList& GetFSSystemUniforms() const{};// = 0;
		virtual const ShaderUniformBufferDeclaration* GetVSUserUniformBuffer() const{};// = 0;
		virtual const ShaderUniformBufferDeclaration* GetFSUserUniformBuffer() const{}; ;//= 0;

		virtual const ShaderResourceList& GetResources() const {};// = 0;

		virtual const std::string& GetName() const{}// = 0;
		virtual const std::string& GetFilepath() const{}// = 0 {};

		// Creates a shader from a vertex and fragment source
		static Ref<Shader> Create(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);

		// Creates a shader from a file
		static Ref<Shader> Create(const std::string& filepath);

		virtual void SetUniformFloat(const std::string& name, float value) {};// = 0;
		virtual void SetUniformFloat2(const std::string& name, const glm::vec2& value) {};// = 0;
		virtual void SetUniformFloat3(const std::string& name, const glm::vec3& value) {};// = 0;
		virtual void SetUniformFloat4(const std::string& name, const glm::vec4& value) {};// = 0;
		virtual void SetUniformInt(const std::string& name, int value) {};// = 0;
		virtual void SetUniformInt2(const std::string& name, int a, int b) {};// = 0;
		virtual void SetUniformIntV(const std::string& name, int* ptr, uint32_t count) {};// = 0;
		virtual void SetUniformMat3(const std::string& name, const glm::mat3& value) {};// = 0;
		virtual void SetUniformMat4(const std::string& name, const glm::mat4& value) {};// = 0;
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