#pragma once

#include "Crowny/Renderer/Shader.h"

#include <glm/glm.hpp>

namespace Crowny
{

	class OpenGLResourceDeclaration : public ShaderResourceDeclaration
	{
	public:
		enum class Type
		{
			NONE, TEXTURE2D, TEXTURECUBE, TEXTURESHADOW
		};
	private:
		std::string m_Name;
		uint32_t m_Register;
		uint32_t m_Count;
		Type m_Type;

		friend class OpenGLShader;

	public:
		OpenGLResourceDeclaration(Type type, const std::string& name, uint32_t count);
		virtual const std::string& GetName() const override { return m_Name; }
		virtual uint32_t GetRegister() const override { return m_Register; }
		virtual uint32_t GetCount() const override { return m_Count; }

		Type GetType() const { return m_Type; }
		static Type StringToType(const std::string& type);
	};

	class OpenGLUniformDeclaration : public ShaderUniformDeclaration
	{
	public:
		enum class Type
		{
			NONE, FLOAT32, VEC2, VEC3, VEC4, MAT3, MAT4, INT32, STRUCT
		};
	private:
		friend class OpenGLShader;

		std::string m_Name;
		uint32_t m_Size;
		uint32_t m_Count;
		uint32_t m_Offset;
		Type m_Type;

		ShaderStruct* m_Struct;
		mutable int32_t m_Location;

		OpenGLUniformDeclaration(Type type, const std::string& name, uint32_t count = 1);
		OpenGLUniformDeclaration(ShaderStruct* uniformStruct, const std::string& name, uint32_t count = 1);

	public:
		virtual const std::string& GetName() const override { return m_Name; }
		virtual uint32_t GetSize() const override { return m_Size; }
		virtual uint32_t GetOffset() const override { return m_Offset; }
		virtual uint32_t GetCount() const override { return m_Count; }
		virtual uint32_t GetAbsoluteOffset() const override { return m_Struct ? m_Struct->GetOffset() + m_Offset : m_Offset; }

		uint32_t GetLocation() const { return m_Location; }
		Type GetType() const { return m_Type; }
		const ShaderStruct& GetShaderUniformStruct() const { CW_ENGINE_ASSERT(m_Struct); return *m_Struct; }

		void SetOffset(uint32_t offset) override;
	public:
		static uint32_t SizeOfUniformType(Type type);
		static Type StringToType(const std::string& type);
	};

	struct OpenGLUniformField
	{
		OpenGLUniformDeclaration::Type Type;
		std::string Name;
		uint32_t Count;
		mutable uint32_t Size;
		mutable int32_t Location;
	};

	class OpenGLUniformBufferDeclaration : public ShaderUniformBufferDeclaration
	{
	private:
		std::string m_Name;
		ShaderUniformList m_Uniforms;
		uint32_t m_Register;
		uint32_t m_Size;
		uint32_t m_ShaderType;

	public:
		OpenGLUniformBufferDeclaration(const std::string& name, uint32_t shaderType);
		void PushUniform(OpenGLUniformDeclaration* uniform);

		virtual const std::string& GetName() const override { return m_Name; }
		virtual uint32_t GetRegister() const override { return m_Register; }
		virtual uint32_t GetShaderType() const override { return m_ShaderType; }
		virtual uint32_t GetSize() const override { return m_Size; }
		virtual const ShaderUniformList& GetUniformDeclarations() const override { return m_Uniforms; }

		ShaderUniformDeclaration* FindUniform(const std::string& name) override;
	};

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
		OpenGLShader(const std::string& filepath);
		~OpenGLShader();

		virtual void Reload() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void SetVSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot) override;
		virtual void SetFSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot) override;

		virtual void SetVSUserUniformBuffer(byte* data, uint32_t size) override;
		virtual void SetFSUserUniformBuffer(byte* data, uint32_t size) override;

		void SetUniform(const std::string& name, byte* data);
		void ResolveAndSetUniformField(const OpenGLUniformDeclaration& field, byte* data, uint32_t offset);

		virtual const std::string& GetName() const override { return m_Name; };
		virtual const std::string& GetFilepath() const override { return m_Filepath; }

		virtual const ShaderResourceList& GetResources() const override { return m_Resources; }
		virtual const ShaderUniformBufferList& GetVSSystemUniforms() const override { return m_VSUniformBuffers; }
		virtual const ShaderUniformBufferList& GetFSSystemUniforms() const override { return m_FSUniformBuffers; }
		virtual const ShaderUniformBufferDeclaration* GetVSUserUniformBuffer() const override { return m_VSUserUniformBuffer; }
		virtual const ShaderUniformBufferDeclaration* GetFSUserUniformBuffer() const override { return m_FSUserUniformBuffer; }

	private:
		bool IsTypeStringResource(const std::string& type);
		ShaderStruct* FindStruct(const std::string& name);
		ShaderUniformDeclaration* FindUniformDeclaration(const std::string& name, const ShaderUniformBufferDeclaration* buff);
		ShaderUniformDeclaration* FindUniformDeclaration(const std::string& name);

		void ResolveAndSetUniforms(ShaderUniformBufferDeclaration* buff, byte* data, uint32_t size);
		void ResolveAndSetUniform(OpenGLUniformDeclaration* uniform, byte* data, uint32_t size);

		virtual void SetUniformFloat(const std::string& name, float value) override;
		virtual void SetUniformFloat2(const std::string& name, const glm::vec2& value) override;
		virtual void SetUniformFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void SetUniformFloat4(const std::string& name, const glm::vec4& value) override;
		virtual void SetUniformInt(const std::string& name, int value) override;
		virtual void SetUniformInt2(const std::string& name, int a, int b) override;
		virtual void SetUniformIntV(const std::string& name, int* ptr, uint32_t count) override;
		virtual void SetUniformMat3(const std::string& name, const glm::mat3& value) override;
		virtual void SetUniformMat4(const std::string& name, const glm::mat4& value) override;

		void SetUniformFloat(uint32_t location, float value);
		void SetUniformFloat2(uint32_t location, const glm::vec2& value);
		void SetUniformFloat3(uint32_t location, const glm::vec3& value);
		void SetUniformFloat4(uint32_t location, const glm::vec4& value);
		void SetUniformInt(uint32_t location, int value);
		void SetUniformInt2(uint32_t location, int a, int b);
		void SetUniformIntV(uint32_t location, int* ptr, uint32_t count);
		void SetUniformMat3(uint32_t location, const glm::mat3& value);
		void SetUniformMat4(uint32_t location, const glm::mat4& value);
		void SetUniformStruct(OpenGLUniformDeclaration* uniform, byte* data, int32_t offset);

		uint32_t GetUniformLocation(const std::string& name);

		void Load(const std::string& path);
		std::unordered_map<uint32_t, std::string> ShaderPreProcess(const std::string& source);
		void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);
		void Parse(const std::string& vertSrc, const std::string& fragSrc);
		void ParseUniform(const std::string& statement, uint32_t shaderType);
		void ParseUniformStruct(const std::string& block, uint32_t shaderType);
		void ResolveUniforms();

	private:
		ShaderUniformBufferList m_VSUniformBuffers;
		ShaderUniformBufferList m_FSUniformBuffers;
		OpenGLUniformBufferDeclaration* m_VSUserUniformBuffer = nullptr;
		OpenGLUniformBufferDeclaration* m_FSUserUniformBuffer = nullptr;
		ShaderResourceList m_Resources;
		ShaderStructList m_Structs;

		std::string m_Filepath;
		std::string m_Name;
		uint32_t m_RendererID;
	};

}
