#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{
    // Sample ABI v1. Callers supply ray/surface derivatives explicitly, including
    // in compute where screen-space derivative instructions are unavailable.
    struct OslTextureSample
    {
        glm::vec4 UVTime = glm::vec4(0);      // u, v, time, reserved
        glm::vec4 Derivatives = glm::vec4(0); // dudx, dudy, dvdx, dvdy
    };
    static_assert(sizeof(OslTextureSample) == 32);

    // Experimental procedural evaluator. Material closures will use a separate
    // result ABI rather than reinterpreting this color output as a BSDF.
    class OslTextureProgram
    {
    public:
        bool Initialize(const Vector<uint8_t>& validatedSpirv, String& error);
        bool Dispatch(const Ref<GenericGpuBuffer>& samples, const Ref<GenericGpuBuffer>& colors, String& error,
                      const Ref<CommandBuffer>& commandBuffer = nullptr);

    private:
        Ref<ComputePipeline> m_Pipeline;
        Ref<UniformParams> m_Uniforms;
    };
} // namespace Crowny
