#include "cwpch.h"

#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        GLuint LinkProgram(std::initializer_list<Ref<ShaderStage>> stages)
        {
            const GLuint program = glCreateProgram();
            for (const Ref<ShaderStage>& stage : stages)
            {
                if (!stage)
                    continue;
                const OpenGLShader* shader = static_cast<const OpenGLShader*>(stage.get());
                if (!shader->IsValid())
                {
                    glDeleteProgram(program);
                    throw std::runtime_error("Cannot link an OpenGL pipeline with an invalid shader stage");
                }
                glAttachShader(program, shader->GetRendererID());
            }
            glLinkProgram(program);

            GLint linked = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE)
            {
                GLint length = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                String log(static_cast<size_t>(std::max(length, 1)), '\0');
                glGetProgramInfoLog(program, length, nullptr, log.data());
                glDeleteProgram(program);
                CW_ENGINE_ERROR("OpenGL pipeline link failed: {}", log);
                throw std::runtime_error("OpenGL pipeline link failed: " + log);
            }

            for (const Ref<ShaderStage>& stage : stages)
            {
                if (stage)
                    glDetachShader(program, static_cast<OpenGLShader*>(stage.get())->GetRendererID());
            }
            return program;
        }

        void ConfigureBindings(GLuint program, const Ref<ShaderStage>& stage)
        {
            if (!stage || !stage->GetUniformDesc())
                return;
            const Ref<UniformDesc>& desc = stage->GetUniformDesc();
            for (const auto& [name, block] : desc->Uniforms)
            {
                const GLuint index = glGetUniformBlockIndex(program, name.c_str());
                if (index != GL_INVALID_INDEX)
                    glUniformBlockBinding(program, index, OpenGLUtils::FlattenBinding(block.Set, block.Slot));
            }
            GLint previousProgram = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
            glUseProgram(program);
            for (const auto& [name, texture] : desc->Textures)
            {
                const GLint location = glGetUniformLocation(program, name.c_str());
                if (location >= 0)
                    glUniform1i(location, static_cast<GLint>(OpenGLUtils::FlattenBinding(texture.Set, texture.Slot)));
            }
            for (const auto& [name, image] : desc->LoadStoreTextures)
            {
                const GLint location = glGetUniformLocation(program, name.c_str());
                if (location >= 0)
                    glUniform1i(location, static_cast<GLint>(OpenGLUtils::FlattenBinding(image.Set, image.Slot)));
            }
            if (GLAD_GL_VERSION_4_3)
            {
                for (const auto& [name, buffer] : desc->Buffers)
                {
                    const GLuint index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, name.c_str());
                    if (index != GL_INVALID_INDEX)
                        glShaderStorageBlockBinding(program, index, OpenGLUtils::FlattenBinding(buffer.Set, buffer.Slot));
                }
            }
            glUseProgram(static_cast<GLuint>(previousProgram));
        }
    } // namespace

    OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const PipelineStateDesc& desc) : GraphicsPipeline(desc)
    {
        CW_ENGINE_ASSERT(desc.VertexShader != nullptr, "OpenGL graphics pipelines require a vertex shader");
        CW_ENGINE_ASSERT(desc.FragmentShader != nullptr, "OpenGL graphics pipelines require a fragment shader");
        if (desc.HullShader || desc.DomainShader)
            CW_ENGINE_ASSERT(desc.HullShader && desc.DomainShader, "OpenGL tessellation control and evaluation shaders must be paired");

        m_Program = LinkProgram({ desc.VertexShader, desc.FragmentShader, desc.GeometryShader, desc.HullShader, desc.DomainShader });
        ConfigureBindings(m_Program, desc.VertexShader);
        ConfigureBindings(m_Program, desc.FragmentShader);
        ConfigureBindings(m_Program, desc.GeometryShader);
        ConfigureBindings(m_Program, desc.HullShader);
        ConfigureBindings(m_Program, desc.DomainShader);
    }

    OpenGLGraphicsPipeline::~OpenGLGraphicsPipeline()
    {
        if (m_Program != 0)
            glDeleteProgram(m_Program);
    }

    OpenGLComputePipeline::OpenGLComputePipeline(const Ref<ShaderStage>& shader) : ComputePipeline(shader)
    {
        if (!GLAD_GL_VERSION_4_3)
            throw std::runtime_error("OpenGL compute shaders require OpenGL 4.3 or newer");
        m_Program = LinkProgram({ shader });
        ConfigureBindings(m_Program, shader);
    }

    OpenGLComputePipeline::~OpenGLComputePipeline()
    {
        if (m_Program != 0)
            glDeleteProgram(m_Program);
    }
} // namespace Crowny
