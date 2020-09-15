#include "cwpch.h"
#include "OpenGLShader.h"
#include "Crowny/Common/VirtualFileSystem.h"
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
		if (!m_Uniforms.empty())
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
		Parse(sources[GL_VERTEX_SHADER], sources[GL_FRAGMENT_SHADER]);
		Compile(sources);
		ResolveUniforms();
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_RendererID);
	}

	void OpenGLShader::Load(const std::string& filepath)
	{
		std::string source = VirtualFileSystem::Get()->ReadTextFile(filepath);
		auto shaderSources = ShaderPreProcess(source);
		Parse(shaderSources[GL_VERTEX_SHADER], shaderSources[GL_FRAGMENT_SHADER]);
		Compile(shaderSources);
		ResolveUniforms();

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

	static GLenum ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")
			return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel")
			return GL_FRAGMENT_SHADER;

		CW_ENGINE_ASSERT(false, "Unknown shader type!");
		return 0;
	}

	std::unordered_map<GLenum, std::string> OpenGLShader::ShaderPreProcess(const std::string& source)
	{
		std::unordered_map<GLenum, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			CW_ENGINE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			CW_ENGINE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			CW_ENGINE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos);

			shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}
		return shaderSources;
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
		m_VSUniformBuffers.push_back(new OpenGLUniformBufferDeclaration("Global", GL_VERTEX_SHADER));
		m_FSUniformBuffers.push_back(new OpenGLUniformBufferDeclaration("Global", GL_FRAGMENT_SHADER));

		const char* token;
		const char* vstr = vertSrc.c_str();;
		const char* fstr = fragSrc.c_str();

		while (token = FindToken(vstr, "struct"))
			ParseUniformStruct(GetBlock(token, &vstr), GL_VERTEX_SHADER);

		while (token = FindToken(vstr, "uniform"))
			ParseUniform(GetStatement(token, &vstr), GL_VERTEX_SHADER);

		while (token = FindToken(fstr, "struct"))
			ParseUniform(GetBlock(token, &fstr), GL_FRAGMENT_SHADER);

		while (token = FindToken(fstr, "uniform"))
			ParseUniform(GetStatement(token, &fstr), GL_FRAGMENT_SHADER);
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
				if (shaderType == GL_VERTEX_SHADER)
				{
					if (m_VSUserUniformBuffer == nullptr)
						m_VSUserUniformBuffer = new OpenGLUniformBufferDeclaration("", 0);

					m_VSUserUniformBuffer->PushUniform(decl);
				}
				else if (shaderType == GL_FRAGMENT_SHADER)
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

	ShaderUniformDeclaration* OpenGLShader::FindUniformDeclaration(const std::string& name)
	{
		ShaderUniformDeclaration* res = nullptr;
		for (auto* ubuff : m_VSUniformBuffers)
		{
			res = FindUniformDeclaration(name, ubuff);
			if (res) 
				return res;
		}

		for (auto* ubuff : m_FSUniformBuffers)
		{
			res = FindUniformDeclaration(name, ubuff);
			if (res)
				return res;
		}

		res = FindUniformDeclaration(name, m_VSUserUniformBuffer);
		if (res)
			return res;

		res = FindUniformDeclaration(name, m_FSUserUniformBuffer);
		if (res)
			return res;

		return res;
	}

	ShaderUniformDeclaration* OpenGLShader::FindUniformDeclaration(const std::string& name, const ShaderUniformBufferDeclaration* buff)
	{
		const ShaderUniformList& uniforms = buff->GetUniformDeclarations();
		for (auto* uniform : uniforms)
		{
			if (uniform->GetName() == name)
				return uniform;
		}

		return nullptr;
	}

	void OpenGLShader::ResolveUniforms()
	{
		Bind();

		for (auto* VSUniformBuffer : m_VSUniformBuffers)
		{
			OpenGLUniformBufferDeclaration* decl = (OpenGLUniformBufferDeclaration*)VSUniformBuffer;
			const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
			for (auto* j : uniforms)
			{
				OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)j;
				if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
				{
					const ShaderStruct& s = uniform->GetShaderUniformStruct();
					const auto& fields = s.GetFields();
					for (auto* k : fields)
					{
						OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)k;
						field->m_Location = GetUniformLocation(uniform->m_Name + "." + field->m_Name);
					}
				}
				else
				{
					uniform->m_Location = GetUniformLocation(uniform->m_Name);
				}
			}

			for (auto* FSUniformBuffer : m_FSUniformBuffers)
			{
				OpenGLUniformBufferDeclaration* decl = (OpenGLUniformBufferDeclaration*)FSUniformBuffer;
				const ShaderUniformList& uniforms = decl->GetUniformDeclarations();
				for (auto* j : uniforms)
				{
					OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)j;
					if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
					{
						const ShaderStruct& s = uniform->GetShaderUniformStruct();
						const auto& fields = s.GetFields();
						for (auto* k : fields)
						{
							OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)k;
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
					for (auto* j : uniforms)
					{
						OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)j;
						if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
						{
							const ShaderStruct& s = uniform->GetShaderUniformStruct();
							const auto& fields = s.GetFields();
							for (auto* k : fields)
							{
								OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)k;
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
					for (auto* j : uniforms)
					{
						OpenGLUniformDeclaration* uniform = (OpenGLUniformDeclaration*)j;
						if (uniform->GetType() == OpenGLUniformDeclaration::Type::STRUCT)
						{
							const ShaderStruct& s = uniform->GetShaderUniformStruct();
							const auto& fields = s.GetFields();
							for (auto* k : fields)
							{
								OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)k;
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

			uint32_t sampler = 0;
			for (auto* rs : m_Resources)
			{
				OpenGLResourceDeclaration* decl = (OpenGLResourceDeclaration*)rs;
				uint32_t location = GetUniformLocation(decl->m_Name);
				if (decl->GetCount() == 1)
				{
					decl->m_Register = sampler;
					SetUniformInt(location, sampler++);
				}
				else if (decl->GetCount() > 1)
				{
					decl->m_Register = 0;
					uint32_t count = decl->GetCount();
					int32_t* samplers = new int32_t[count];
					for (uint8_t i = 0; i < count; i++)
						samplers[i] = i;
					SetUniformIntV(decl->GetName(), samplers, count);
					delete[] samplers;
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

	void OpenGLShader::SetVSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot)
	{
		Bind();
		ShaderUniformBufferDeclaration* decl = m_VSUniformBuffers[slot];
		ResolveAndSetUniforms(decl, data, size);
	}

	void OpenGLShader::SetFSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot)
	{
		Bind();
		ShaderUniformBufferDeclaration* decl = m_FSUniformBuffers[slot];
		ResolveAndSetUniforms(decl, data, size);
	}

	void OpenGLShader::SetVSUserUniformBuffer(byte* data, uint32_t size)
	{
		ResolveAndSetUniforms(m_VSUserUniformBuffer, data, size);
	}

	void OpenGLShader::SetFSUserUniformBuffer(byte* data, uint32_t size)
	{
		ResolveAndSetUniforms(m_FSUserUniformBuffer, data, size);
	}

	void OpenGLShader::ResolveAndSetUniforms(ShaderUniformBufferDeclaration* buff, byte* data, uint32_t size)
	{
		const ShaderUniformList& uniforms = buff->GetUniformDeclarations();
		for (auto* uniform : uniforms)
		{
			ResolveAndSetUniform((OpenGLUniformDeclaration*)uniform, data, size);
		}
	}

	void OpenGLShader::ResolveAndSetUniform(OpenGLUniformDeclaration* uniform, byte* data, uint32_t size)
	{
		if (uniform->GetLocation() == -1) 
			return;

		uint32_t offset = uniform->GetOffset();
		switch (uniform->GetType())
		{
		case OpenGLUniformDeclaration::Type::FLOAT32:
			SetUniformFloat(uniform->GetLocation(), *(float*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::INT32:
			SetUniformInt(uniform->GetLocation(), *(int*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::VEC2:
			SetUniformFloat2(uniform->GetLocation(), *(glm::vec2*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::VEC3:
			SetUniformFloat3(uniform->GetLocation(), *(glm::vec3*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::VEC4:
			SetUniformFloat4(uniform->GetLocation(), *(glm::vec4*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::MAT3:
			SetUniformMat3(uniform->GetLocation(), *(glm::mat3*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::MAT4:
			SetUniformMat4(uniform->GetLocation(), *(glm::mat4*)&data[offset]); break;
		case OpenGLUniformDeclaration::Type::STRUCT:
			SetUniformStruct(uniform, data, offset); break;
		}
	}

	void OpenGLShader::SetUniform(const std::string& name, byte* data)
	{
		ShaderUniformDeclaration* uniform = FindUniformDeclaration(name);
		if (!uniform)
		{
			return;
		}
		ResolveAndSetUniform((OpenGLUniformDeclaration*)uniform, data, 0);
	}

	void OpenGLShader::ResolveAndSetUniformField(const OpenGLUniformDeclaration& field, byte* data, uint32_t offset)
	{
		switch (field.GetType())
		{
		case OpenGLUniformDeclaration::Type::FLOAT32:
			SetUniformFloat(field.GetLocation(), *(float*)&data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::INT32:
			SetUniformInt(field.GetLocation(), *(int32_t*)&data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::VEC2:
			SetUniformFloat2(field.GetLocation(), *(glm::vec2*) & data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::VEC3:
			SetUniformFloat3(field.GetLocation(), *(glm::vec3*) & data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::VEC4:
			SetUniformFloat4(field.GetLocation(), *(glm::vec4*) & data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::MAT3:
			SetUniformMat3(field.GetLocation(), *(glm::mat3*)&data[offset]);
			break;
		case OpenGLUniformDeclaration::Type::MAT4:
			SetUniformMat4(field.GetLocation(), *(glm::mat4*) & data[offset]);
			break;
		default:
			CW_ENGINE_ASSERT(false, "Unknown type!");
		}
	}

	void OpenGLShader::SetUniformInt(const std::string& name, int value)
	{
		SetUniformInt(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformIntV(const std::string& name, int* ptr, uint32_t count)
	{
		SetUniformIntV(GetUniformLocation(name), ptr, count);
	}

	void OpenGLShader::SetUniformFloat(const std::string& name, float value)
	{
		SetUniformFloat(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		SetUniformFloat2(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		SetUniformFloat3(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformFloat4(const std::string& name, const glm::vec4& value)
	{
		SetUniformFloat4(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformMat3(const std::string& name, const glm::mat3& value)
	{
		SetUniformMat3(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformMat4(const std::string& name, const glm::mat4& value)
	{
		SetUniformMat4(GetUniformLocation(name), value);
	}

	void OpenGLShader::SetUniformInt(uint32_t location, int value)
	{
		glUniform1i(location, value);
	}

	void OpenGLShader::SetUniformIntV(uint32_t location, int* ptr, uint32_t count)
	{
		glUniform1iv(location, count, ptr);
	}

	void OpenGLShader::SetUniformFloat(uint32_t location, float value)
	{
		glUniform1f(location, value);
	}

	void OpenGLShader::SetUniformFloat2(uint32_t location, const glm::vec2& value)
	{
		glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::SetUniformFloat3(uint32_t location, const glm::vec3& value)
	{
		glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::SetUniformFloat4(uint32_t location, const glm::vec4& value)
	{
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::SetUniformMat3(uint32_t location, const glm::mat3& matrix)
	{
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::SetUniformMat4(uint32_t location, const glm::mat4& matrix)
	{
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::SetUniformStruct(OpenGLUniformDeclaration* uniform, byte* data, int32_t offset)
	{
		const ShaderStruct& ss = uniform->GetShaderUniformStruct();
		const auto& fields = ss.GetFields();

		for (auto& f : fields)
		{
			OpenGLUniformDeclaration* field = (OpenGLUniformDeclaration*)f;
			ResolveAndSetUniformField(*field, data, offset);
			offset += field->m_Size;
		}
	}

	uint32_t OpenGLShader::GetUniformLocation(const std::string& name)
	{
		uint32_t result = glGetUniformLocation(m_RendererID, name.c_str());
		CW_ENGINE_ASSERT(result != -1, "Could not find uniform declaration: " + name);
		return result;
	}

}