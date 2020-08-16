#include "cwpch.h"
#include "OpenGLShader.h"
#include "Crowny/Common/FileUtils.h"
#include "Crowny/Common/Parser.h"

#ifdef MC_WEB
#include <GLES3/gl32.h>
#else
#include <glad/glad.h>
#endif

#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

	OpenGLResourceDeclaration::Type OpenGLResourceDeclaration::StringToType(const std::string& type)
	{
		if (type == "sampler2D")		return Type::TEXTURE2D;
		if (type == "samplerCube")		return Type::TEXTURECUBE;
		if (type == "samplerShadow")	return Type::TEXTURESHADOW;

		return Type::NONE;
	}

	OpenGLUniformDeclaration::Type OpenGLUniformDeclaration::StringToType(const std::string& type)
	{
		if (type == "int32")		return OpenGLUniformDeclaration::Type::INT32;
		if (type == "float")		return OpenGLUniformDeclaration::Type::FLOAT32;
		if (type == "vec2")			return OpenGLUniformDeclaration::Type::VEC2;
		if (type == "vec3")			return OpenGLUniformDeclaration::Type::VEC3;
		if (type == "vec4")			return OpenGLUniformDeclaration::Type::VEC4;
		if (type == "mat3")			return OpenGLUniformDeclaration::Type::MAT3;
		if (type == "mat4")			return OpenGLUniformDeclaration::Type::MAT4;

		return OpenGLUniformDeclaration::Type::NONE;
	}

	OpenGLResourceDeclaration::OpenGLResourceDeclaration(Type type, const std::string& name, uint32_t count) : m_Type(type), m_Name(name), m_Count(count)
	{

	}

	OpenGLUniformDeclaration::OpenGLUniformDeclaration(Type type, const std::string& name, uint32_t count) : m_Type(type), m_Name(name), m_Count(count), m_Struct(nullptr)
	{
		m_Size = SizeOfUniformType(type) * count;
	}

	OpenGLUniformDeclaration::OpenGLUniformDeclaration(ShaderStruct* uniformStruct, const std::string& name, uint32_t count) : m_Struct(uniformStruct), m_Name(name), m_Count(count), m_Type(OpenGLUniformDeclaration::Type::STRUCT)
	{
		m_Size = m_Struct->GetSize() * count;
	}

	void OpenGLUniformDeclaration::SetOffset(uint32_t offset)
	{
		if (m_Type == OpenGLUniformDeclaration::Type::STRUCT)
		{
			m_Struct->SetOffset(offset);
		}

		m_Offset = offset;
	}

	uint32_t OpenGLUniformDeclaration::SizeOfUniformType(Type type)
	{
		switch (type)
		{
		case OpenGLUniformDeclaration::Type::INT32:            return 4;
		case OpenGLUniformDeclaration::Type::FLOAT32:          return 4;
		case OpenGLUniformDeclaration::Type::VEC2:             return 4 * 2;
		case OpenGLUniformDeclaration::Type::VEC3:             return 4 * 3;
		case OpenGLUniformDeclaration::Type::VEC4:	           return 4 * 4;
		case OpenGLUniformDeclaration::Type::MAT3:             return 4 * 3 * 3;
		case OpenGLUniformDeclaration::Type::MAT4:             return 4 * 4 * 4;
		}
		CW_ENGINE_ASSERT(false, "Invalid uniform type");
		return 0;
	}

	OpenGLUniformBufferDeclaration::OpenGLUniformBufferDeclaration(const std::string& name, uint32_t shaderType) : m_Name(name), m_ShaderType(shaderType), m_Size(0), m_Register(0)
	{

	}

	void OpenGLUniformBufferDeclaration::PushUniform(OpenGLUniformDeclaration* uniform)
	{
		uint32_t offset = 0;
		if (m_Uniforms.size())
		{
			OpenGLUniformDeclaration* prev = (OpenGLUniformDeclaration*)m_Uniforms.back();
			offset = prev->GetOffset() + prev->GetSize();
		}
		uniform->SetOffset(offset);
		m_Size += uniform->GetSize();
		m_Uniforms.push_back(uniform);
	}

	ShaderUniformDeclaration* OpenGLUniformBufferDeclaration::FindUniform(const std::string& name)
	{
		for (ShaderUniformDeclaration* uniform : m_Uniforms)
		{
			if (uniform->GetName() == name)
				return uniform;

			return nullptr;
		}
	}

	OpenGLShader::OpenGLShader(const std::string& filepath) : m_Filepath(filepath)
	{
		Load(filepath);
	}


	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc) : m_Filepath("")
	{
		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertSrc;
		sources[GL_FRAGMENT_SHADER] = fragSrc;
		Compile(sources);
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_RendererID);
	}

	void OpenGLShader::Load(const std::string& filepath)
	{
		std::string source = ReadFile(DIRECTORY_PREFIX + filepath);
		auto shaderSources = ShaderPreProcess(source);
		Compile(shaderSources);

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	void OpenGLShader::Reload() // fix this
	{
		if (m_Filepath == "")

			return;
		Load(m_Filepath);
	}

	void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
	{
		GLuint program = glCreateProgram();
		CW_ENGINE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders for now");
		std::array<GLenum, 2> glShaderIDs;
		int glShaderIDIndex = 0;
		for (auto& kv : shaderSources)
		{
			GLenum type = kv.first;
			const std::string& source = kv.second;

			GLuint shader = glCreateShader(type);

			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);

			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				glDeleteShader(shader);

				CW_ENGINE_ERROR("{0}", infoLog.data());
				CW_ENGINE_ASSERT(false, "Shader compilation failure!");
				break;
			}

			glAttachShader(program, shader);
			glShaderIDs[glShaderIDIndex++] = shader;
		}

		m_RendererID = program;

		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
			glDeleteProgram(program);

			for (auto id : glShaderIDs)
				glDeleteShader(id);

			CW_ENGINE_ERROR("{0}", infoLog.data());
			CW_ENGINE_ASSERT(false, "Shader link failure!");
			return;
		}

		for (auto id : glShaderIDs)
		{
			glDetachShader(program, id);
			glDeleteShader(id);
		}
	}

	void OpenGLShader::Parse(const std::string& vertSrc, const std::string& fragSrc)
	{
		const char* token;
		const char* vstr = vertSrc.c_str();;
		const char* fstr = fragSrc.c_str();

		while (token = FindToken(vstr, "struct"))
			ParseUniformStruct(GetBlock(token, &vstr), 0);

		while (token = FindToken(vstr, "uniform"))
			ParseUniform(GetStatement(token, &vstr), 0);

		while (token = FindToken(fstr, "struct"))
			ParseUniform(GetBlock(token, &fstr), 1);

		while (token = FindToken(fstr, "uniform"))
			ParseUniform(GetStatement(token, &fstr), 1);
	}

	void OpenGLShader::ParseUniform(const std::string& statement, uint32_t shaderType)
	{
		std::vector<std::string> tokens = Tokenize(statement);
		uint32_t index = 0;

		index++;
		std::string typeString = tokens[index++];
		std::string name = tokens[index++];
		if (const char* s = strstr(name.c_str(), ";"))
			name = std::string(name.c_str(), s - name.c_str());

		std::string n(name);
		int32_t count = 1;
		const char* namestr = n.c_str();
		if (const char* s = strstr(namestr, "[")) // Array, get the number of elements
		{
			name = std::string(namestr, s - namestr);
			const char* end = strstr(namestr, "]");
			std::string c(s + 1, end - s);
			count = atoi(c.c_str());
		}

		if (IsTypeStringResource(typeString))
		{
			ShaderResourceDeclaration* decl = new OpenGLResourceDeclaration(OpenGLResourceDeclaration::StringToType(typeString), name, count);
			m_Resources.push_back(decl);
		}
		else
		{
			OpenGLUniformDeclaration::Type t = OpenGLUniformDeclaration::StringToType(typeString);
			OpenGLUniformDeclaration* decl = nullptr;
			if (t == OpenGLUniformDeclaration::Type::NONE)
			{
				ShaderStruct* s = FindStruct(typeString);
				CW_ENGINE_ASSERT(s, "");
				decl = new OpenGLUniformDeclaration(s, name, count);
			}
			else
			{
				decl = new OpenGLUniformDeclaration(t, name, count);
			}


			if (name.rfind("cw_", 0) == 0)
			{
				if (shaderType == GL_VERTEX_SHADER)
					((OpenGLUniformBufferDeclaration*)m_VSUniformBuffers.front())->PushUniform(decl);
				else if (shaderType == GL_FRAGMENT_SHADER)
					((OpenGLUniformBufferDeclaration*)m_FSUniformBuffers.front())->PushUniform(decl);
			}
			else
			{
				if (shaderType == 0)
				{
					if (m_VSUserUniformBuffer == nullptr)
						m_VSUserUniformBuffer = new OpenGLUniformBufferDeclaration("", 0);

					m_VSUserUniformBuffer->PushUniform(decl);
				}
				else if (shaderType == 1)
				{
					if (m_FSUserUniformBuffer == nullptr)
						m_FSUserUniformBuffer = new OpenGLUniformBufferDeclaration("", 1);

					m_FSUserUniformBuffer->PushUniform(decl);
				}
			}
		}
	}

	void OpenGLShader::ParseUniformStruct(const std::string& block, uint32_t shaderType)
	{
		std::vector<std::string> tokens = Tokenize(block);
		uint32_t index = 0;
		index++;
		std::string name = tokens[index++];
		ShaderStruct* uniformStruct = new ShaderStruct(name);
		index++;

		while (index < tokens.size())
		{
			if (tokens[index] == "}")
				break;
			std::string type = tokens[index++];
			std::string name = tokens[index++];

			if (const char* s = strstr(name.c_str(), ";"))
				name = std::string(name.c_str(), s - name.c_str());

			uint32_t count = 1;
			const char* namestr = name.c_str();
			if (const char* s = strstr(namestr, "["))
			{
				name = std::string(namestr, s - namestr);;
				const char* end = strstr(namestr, "]");
				std::string c(s + 1, end - s);
				count = atoi(c.c_str());
			}

			ShaderUniformDeclaration* f = new OpenGLUniformDeclaration(OpenGLUniformDeclaration::StringToType(type), name, count);
			uniformStruct->AddField(f);
		}
		m_Structs.push_back(uniformStruct);
	}

	ShaderStruct* OpenGLShader::FindStruct(const std::string& name)
	{
		for (ShaderStruct* s : m_Structs)
		{
			if (s->GetName() == name)
				return s;
		}

		return nullptr;
	}

	void OpenGLShader::ResolveUniforms()
	{
		Bind();
		for (uint32_t i = 0; i < m_VSUniformBuffers.size(); i++)
		{
			OpenGLUniformBufferDeclaration* decl = (OpenGLUniformBufferDeclaration*)m_VSUniformBuffers[i];
			const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
			for (uint32_t j = 0; j < uniforms.size(); j++)
			{
				OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)uniforms[j];
				if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
				{
					const ShaderStruct& s = uniform->GetShaderUniformStruct();
					const auto& fields = s.GetFields();
					for (uint32_t k = 0; k < fields.size(); k++)
					{
						OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)fields[k];
						field->m_Location = GetUniformLocation(uniform->m_Name + "." + field->m_Name);
					}
				}
				else
				{
					uniform->m_Location = GetUniformLocation(uniform->m_Name);
				}
			}

			for (uint32_t i = 0; i < m_FSUniformBuffers.size(); i++)
			{
				OpenGLUniformBufferDeclaration* decl = (OpenGLUniformBufferDeclaration*)m_FSUniformBuffers[i];
				const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
				for (uint32_t j = 0; j < uniforms.size(); j++)
				{
					OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)uniforms[j];
					if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
					{
						const ShaderStruct& s = uniform->GetShaderUniformStruct();
						const auto& fields = s.GetFields();
						for (uint32_t k = 0; k < fields.size(); k++)
						{
							OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)fields[k];
							field->m_Location = GetUniformLocation(uniform->m_Name + "." + field->m_Name);
						}
					}
					else
					{
						uniform->m_Location = GetUniformLocation(uniform->m_Name);
					}
				}
			}
			{
				OpenGLUniformBufferDeclaration* decl = m_VSUserUniformBuffer;
				if (decl)
				{
					const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
					for (uint32_t j = 0; j < uniforms.size(); j++)
					{
						OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)uniforms[j];
						if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
						{
							const ShaderStruct& s = uniform->GetShaderUniformStruct();
							const auto& fields = s.GetFields();
							for (uint32_t k = 0; k < fields.size(); k++)
							{
								OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)fields[k];
								field->m_Location = GetUniformLocation(uniform->m_Name + "." + field->m_Name);
							}
						}
						else
						{
							uniform->m_Location = GetUniformLocation(uniform->m_Name);
						}
					}
				}
			}

			{
				OpenGLUniformBufferDeclaration* decl = m_FSUserUniformBuffer;
				if (decl)
				{
					const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
					for (uint32_t j = 0; j < uniforms.size(); j++)
					{
						OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)uniforms[j];
						if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
						{
							const ShaderStruct& s = uniform->GetShaderUniformStruct();
							const auto& fields = s.GetFields();
							for (uint32_t k = 0; k < fields.size(); k++)
							{
								OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)fields[k];
								field->m_Location = GetUniformLocation(uniform->m_Name + "." + field->m_Name);
							}
						}
						else
						{
							uniform->m_Location = GetUniformLocation(uniform->m_Name);
						}
					}
				}
			}
		}
	}

	bool OpenGLShader::IsTypeStringResource(const std::string& type)
	{
		if (type == "sampler2D")		return true;
		if (type == "samplerCube")		return true;
		if (type == "sampler2DShadow")	return true;
		return false;
	}

	void OpenGLShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::RetrieveLocations(const std::vector<std::string>& uniforms)
	{
		for (auto& uniform : uniforms)
		{
			m_UniformLocations[uniform] = glGetUniformLocation(m_RendererID, uniform.c_str());
		}
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetIntV(const std::string& name, uint32_t count, int* ptr)
	{
		UploadUniformIntV(name, count, ptr);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		UploadUniformMat4(name, value);
	}

	void OpenGLShader::UploadUniformIntV(const std::string& name, int count, int* ptr)
	{
		glUniform1iv(GetUniformLocation(name), count, ptr);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		glUniform1i(GetUniformLocation(name), value);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		glUniform1f(GetUniformLocation(name), value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		glUniform2f(GetUniformLocation(name), value.x, value.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
	{
		glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	uint32_t OpenGLShader::GetUniformLocation(const std::string& name)
	{
		if (m_UniformLocations.find(name) == m_UniformLocations.end())
		{
			m_UniformLocations[name] = glGetUniformLocation(m_RendererID, name.c_str());
		}
		return m_UniformLocations[name];
	}

}