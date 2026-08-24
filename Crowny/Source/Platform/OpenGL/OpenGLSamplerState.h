#pragma once

#include "Crowny/RenderAPI/SamplerState.h"

namespace Crowny
{
    class OpenGLSamplerState : public SamplerState
    {
    public:
        friend class SamplerState;
        ~OpenGLSamplerState() override;

        uint32_t GetRendererID() const { return m_RendererID; }

    protected:
        explicit OpenGLSamplerState(const SamplerStateDesc& desc);

    private:
        uint32_t m_RendererID = 0;
    };
} // namespace Crowny
