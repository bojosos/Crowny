#pragma once

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class OpenGLShader : public Shader
    {
    public:
        OpenGLShader(const String& name, const String& vertSrc, const String& fragSrc);
        OpenGLShader(const Path& filepath);
        OpenGLShader(const BinaryShaderData& data) {}
        ~OpenGLShader();

        virtual const Ref<UniformDesc>& GetUniformDesc() const override { return m_UniformDesc; }

    private:
        void Load(const Path& path);
        UnorderedMap<uint32_t, String> ShaderPreProcess(const String& source);
        void Compile(const UnorderedMap<uint32_t, String>& shaderSources);

    private:
        Ref<UniformDesc> m_UniformDesc;
        String m_Filepath;
        String m_Name;
        uint32_t m_RendererID;
    };

} // namespace Crowny
