#pragma once

#include <cfloat>

namespace Crowny
{

    class Initializer;

    struct SamplerStateDesc
    {
        SamplerStateDesc() = default;

        TextureFilter MinFilter = TextureFilter::LINEAR;
        TextureFilter MagFilter = TextureFilter::LINEAR;
        TextureFilter MipFilter = TextureFilter::LINEAR;
        uint32_t MaxAnsio = 0;
        float MipmapBias = 0;
        float MipMin = -FLT_MAX;
        float MipMax = FLT_MAX;
        TextureAddressingMode AddressMode;
        // TODO: border color
        CompareFunction CompareFunc = CompareFunction::ALWAYS_PASS;
    };

    class SamplerState
    {
    public:
        SamplerState(const SamplerStateDesc& desc);
        virtual ~SamplerState() = default;
        const SamplerStateDesc& GetProperties() const;

    public:
        static Ref<SamplerState> Create(const SamplerStateDesc& desc);
        static const Ref<SamplerState>& GetDefault();

    protected:
        SamplerStateDesc m_Properties;

    private:
        friend class Initializer;
        static Ref<SamplerState> s_DefaultSamplerState;
    };
} // namespace Crowny
