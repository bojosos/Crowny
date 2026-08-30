#pragma once

#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{
    struct OpenGLTextureBindingPlan
    {
        uint32_t FirstUnit = 0;
        uint32_t UnitCount = 0;
        uint32_t AssignedCount = 0;
    };

    OpenGLTextureBindingPlan BuildOpenGLTextureBindingPlan(uint32_t firstUnit, uint32_t maximumTextureUnits, uint32_t reflectedArraySize,
                                                           bool runtimeArray, uint32_t configuredArraySize, bool singleTextureAssigned);

    class OpenGLUniformParamInfo : public UniformParamInfo
    {
    public:
        friend class UniformParamInfo;

    protected:
        explicit OpenGLUniformParamInfo(const UniformParamDesc& desc) : UniformParamInfo(desc) {}
    };

    class OpenGLUniformParams : public UniformParams
    {
    public:
        friend class UniformParams;
        void Bind() const;

    protected:
        explicit OpenGLUniformParams(const Ref<UniformParamInfo>& desc);

    private:
        uint32_t m_MaximumTextureUnits = 0;
    };
} // namespace Crowny
