#pragma once

#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{
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
        explicit OpenGLUniformParams(const Ref<UniformParamInfo>& desc) : UniformParams(desc) {}
    };
} // namespace Crowny
