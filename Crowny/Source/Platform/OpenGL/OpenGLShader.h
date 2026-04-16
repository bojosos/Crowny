#pragma once

#include "Crowny/RenderAPI/Shader.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class OpenGLShader : public ShaderStage
    {
    public:
        friend class ShaderStage;
        ~OpenGLShader();
    protected:
        OpenGLShader(const BinaryShaderData& data) {}

    private:
        uint32_t m_RendererID;
    };

} // namespace Crowny
