#include "cwpch.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include <glad/glad.h>
#include <spirv_cross/spirv_glsl.hpp>

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        GLenum GetShaderType(ShaderType type)
        {
            switch (type)
            {
            case VERTEX_SHADER: return GL_VERTEX_SHADER;
            case FRAGMENT_SHADER: return GL_FRAGMENT_SHADER;
            case GEOMETRY_SHADER: return GL_GEOMETRY_SHADER;
            case HULL_SHADER: return GL_TESS_CONTROL_SHADER;
            case DOMAIN_SHADER: return GL_TESS_EVALUATION_SHADER;
            case COMPUTE_SHADER: return GL_COMPUTE_SHADER;
            default: break;
            }
            throw std::invalid_argument("OpenGL does not support the requested shader stage");
        }

        const char* GetShaderName(ShaderType type)
        {
            switch (type)
            {
            case VERTEX_SHADER: return "vertex shader";
            case FRAGMENT_SHADER: return "fragment shader";
            case GEOMETRY_SHADER: return "geometry shader";
            case HULL_SHADER: return "tessellation control shader";
            case DOMAIN_SHADER: return "tessellation evaluation shader";
            case COMPUTE_SHADER: return "compute shader";
            default: return "shader";
            }
        }

        void RemoveResourceBinding(spirv_cross::CompilerGLSL& compiler, const spirv_cross::Resource& resource)
        {
            if (compiler.has_decoration(resource.id, spv::DecorationBinding))
                compiler.unset_decoration(resource.id, spv::DecorationBinding);
            if (compiler.has_decoration(resource.id, spv::DecorationDescriptorSet))
                compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
        }

        String CompileGLSL(const BinaryShaderData& data)
        {
            if (data.Data.empty() || data.Data.size() % sizeof(uint32_t) != 0)
                throw std::invalid_argument("Cannot create an OpenGL shader from invalid SPIR-V data");

            Vector<uint32_t> words(data.Data.size() / sizeof(uint32_t));
            std::memcpy(words.data(), data.Data.data(), data.Data.size());
            spirv_cross::CompilerGLSL compiler(words);
            const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
            for (const spirv_cross::Resource& resource : resources.uniform_buffers) RemoveResourceBinding(compiler, resource);
            for (const spirv_cross::Resource& resource : resources.storage_buffers) RemoveResourceBinding(compiler, resource);
            for (const spirv_cross::Resource& resource : resources.sampled_images) RemoveResourceBinding(compiler, resource);
            for (const spirv_cross::Resource& resource : resources.separate_images) RemoveResourceBinding(compiler, resource);
            for (const spirv_cross::Resource& resource : resources.separate_samplers) RemoveResourceBinding(compiler, resource);
            for (const spirv_cross::Resource& resource : resources.storage_images) RemoveResourceBinding(compiler, resource);

            compiler.build_combined_image_samplers();
            for (const spirv_cross::CombinedImageSampler& combined : compiler.get_combined_image_samplers())
            {
                const String imageName = compiler.get_name(combined.image_id);
                const String samplerName = compiler.get_name(combined.sampler_id);
                compiler.set_name(combined.combined_id, imageName.empty() ? samplerName : imageName);
            }

            spirv_cross::CompilerGLSL::Options options;
#if defined(CW_MACOSX)
            options.version = 410;
            options.enable_420pack_extension = false;
#else
            options.version = GLAD_GL_VERSION_4_5 ? 450 : 410;
            options.enable_420pack_extension = options.version >= 420;
#endif
            options.es = false;
            options.vulkan_semantics = false;
            options.separate_shader_objects = false;
            compiler.set_common_options(options);
            return compiler.compile();
        }
    } // namespace

    OpenGLShader::OpenGLShader(const Ref<BinaryShaderData>& data) : ShaderStage(data)
    {
        CW_ENGINE_ASSERT(data != nullptr, "OpenGL shader data is null");
        String source;
        try
        {
            source = CompileGLSL(*data);
        }
        catch (const std::exception& exception)
        {
            CW_ENGINE_ERROR("SPIRV-Cross failed to translate the OpenGL {}: {}", GetShaderName(data->Type), exception.what());
            throw;
        }
        const GLenum type = GetShaderType(data->Type);
        m_RendererID = glCreateShader(type);
        const char* sourcePointer = source.c_str();
        const GLint sourceLength = static_cast<GLint>(source.size());
        glShaderSource(m_RendererID, 1, &sourcePointer, &sourceLength);
        glCompileShader(m_RendererID);

        GLint compiled = GL_FALSE;
        glGetShaderiv(m_RendererID, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE)
        {
            GLint length = 0;
            glGetShaderiv(m_RendererID, GL_INFO_LOG_LENGTH, &length);
            String log(static_cast<size_t>(std::max(length, 1)), '\0');
            glGetShaderInfoLog(m_RendererID, length, nullptr, log.data());
            glDeleteShader(m_RendererID);
            m_RendererID = 0;
            throw std::runtime_error(String("OpenGL ") + GetShaderName(data->Type) + " compilation failed: " + log);
        }

        if (data->Type == VERTEX_SHADER)
            m_BufferLayout = CreateRef<BufferLayout>(data->VertexLayout);
    }

    OpenGLShader::~OpenGLShader()
    {
        if (m_RendererID != 0)
            glDeleteShader(m_RendererID);
    }
} // namespace Crowny
