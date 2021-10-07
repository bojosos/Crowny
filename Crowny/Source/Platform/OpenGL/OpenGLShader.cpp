#include "cwpch.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    OpenGLShader::OpenGLShader(const Path& filepath) : m_Filepath(filepath) { Load(filepath); }

    OpenGLShader::OpenGLShader(const String& name, const String& vertSrc, const String& fragSrc) : m_Filepath("")
    {
        UnorderedMap<GLenum, String> sources;
        sources[GL_VERTEX_SHADER] = vertSrc;
        sources[GL_FRAGMENT_SHADER] = fragSrc;
        Compile(sources);
    }

    OpenGLShader::~OpenGLShader() { glDeleteProgram(m_RendererID); }

    void OpenGLShader::Load(const Path& filepath)
    {
        String source = VirtualFileSystem::Get()->ReadTextFile(filepath);
        auto shaderSources = ShaderPreProcess(source);
        Compile(shaderSources);
        m_Name = filepath.filename();
    }

    static GLenum ShaderTypeFromString(const String& type)
    {
        if (type == "vertex")
            return GL_VERTEX_SHADER;
        if (type == "fragment" || type == "pixel")
            return GL_FRAGMENT_SHADER;
        if (type == "compute")
            return GL_COMPUTE_SHADER;

        CW_ENGINE_ASSERT(false, "Unknown shader type!");
        return 0;
    }

    UnorderedMap<GLenum, String> OpenGLShader::ShaderPreProcess(const String& source)
    {
        UnorderedMap<GLenum, String> shaderSources;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);
        while (pos != String::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            CW_ENGINE_ASSERT(eol != String::npos, "Syntax error");
            size_t begin = pos + typeTokenLength + 1;
            String type = source.substr(begin, eol - begin);

            if (type == "compute")
            {
                shaderSources[ShaderTypeFromString(type)] = source.substr(begin + type.size());
                return shaderSources;
            }
            CW_ENGINE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            CW_ENGINE_ASSERT(nextLinePos != String::npos, "Syntax error");
            pos = source.find(typeToken, nextLinePos);

            shaderSources[ShaderTypeFromString(type)] =
              (pos == String::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
        }
        return shaderSources;
    }

    void OpenGLShader::Compile(const UnorderedMap<GLenum, String>& shaderSources)
    {
        GLuint program = glCreateProgram();
        CW_ENGINE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders for now");
        // std::array<GLenum, 2> glShaderIDs;
        Vector<GLenum> glShaderIDs;
        glShaderIDs.resize(shaderSources.size());
        int glShaderIDIndex = 0;
        for (auto& kv : shaderSources)
        {
            GLenum type = kv.first;
            const String& source = kv.second;

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

                Vector<GLchar> infoLog(maxLength);
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
            Vector<GLchar> infoLog(maxLength);
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
    /*
        void OpenGLShader::Bind() const
        {
            glUseProgram(m_RendererID);
        }

        void OpenGLShader::Unbind() const
        {
            glUseProgram(0);
        }
    */
} // namespace Crowny