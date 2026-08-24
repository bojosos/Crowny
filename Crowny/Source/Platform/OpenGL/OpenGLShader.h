#pragma once

#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{
    class OpenGLShader : public ShaderStage
    {
    public:
        friend class ShaderStage;
        ~OpenGLShader() override;

        uint32_t GetRendererID() const { return m_RendererID; }
        bool IsValid() const { return m_RendererID != 0; }

    protected:
        explicit OpenGLShader(const Ref<BinaryShaderData>& data);

    private:
        uint32_t m_RendererID = 0;
    };
} // namespace Crowny
