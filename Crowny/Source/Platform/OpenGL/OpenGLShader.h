#pragma once

#include "Crowny/RenderAPI/Shader.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class OpenGLShader : public ShaderStage
    {
    public:
        OpenGLShader(const BinaryShaderData& data) {}
        ~OpenGLShader();

    private:
        uint32_t m_RendererID;
    };

} // namespace Crowny
