#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/glm.hpp>

namespace Crowny
{
/*
	class OpenGLResourceDeclaration : public ShaderResourceDeclaration
	{
	public:
		enum class Type
		{
			NONE, TEXTURE2D, TEXTURECUBE, TEXTURESHADOW
		};
	private:
		std::string m_Name;
		uint32_t m_Register = 0;
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
		uint32_t m_Size = 0;
		uint32_t m_Count = 0;
		uint32_t m_Offset = 0;
		Type m_Type;

		ShaderStruct* m_Struct;
		mutable int32_t m_Location = 0;

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

		ShaderUniformDeclaration* FindUniform(const std::string& name) overrid
	};*/

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const BinaryShaderData& data) {}
		~OpenGLShader();

		virtual void Reload() override {};

		virtual void Bind() const override;
		virtual void Unbind() const override;
		
		virtual const UniformDescription& GetUniformDesc() const override {}

		virtual const std::string& GetName() const override { return m_Name; };
		virtual const std::string& GetFilepath() const override { return m_Filepath; }

	private:
		uint32_t GetUniformLocation(const std::string& name);

		void Load(const std::string& path);
		std::unordered_map<uint32_t, std::string> ShaderPreProcess(const std::string& source);
		void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);

	private:
		std::string m_Filepath;
		std::string m_Name;
		uint32_t m_RendererID;
	};

}
