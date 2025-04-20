#pragma once

namespace Crowny
{
    struct UniformDesc;

    // TODO: Inherit? Or just different constructors for UniformParamInfo?
    struct UniformParamDesc
    {
        Ref<UniformDesc> FragmentParams;
        Ref<UniformDesc> VertexParams;
        Ref<UniformDesc> GeometryParams;
        Ref<UniformDesc> HullParams;
        Ref<UniformDesc> DomainParams;

        Ref<UniformDesc> ComputeParams;

        Ref<UniformDesc> HitParams;
        Ref<UniformDesc> MissParams;
        Ref<UniformDesc> RaygenParams;
    };

    struct UniformBinding
    {
        uint32_t Set = (uint32_t)-1;
        uint32_t Slot = (uint32_t)-1;
    };

    class UniformParamInfo
    {
    public:
        enum class ParamType
        {
            ParamBlock,
            Texture,
            LoadStoreTexture,
            Buffer,
            SamplerState,
            AccelStruct,
            Count
        };

        UniformParamInfo(const UniformParamDesc& desc);
        virtual ~UniformParamInfo() = default;

        uint32_t GetNumSets() const { return m_NumSets; }

        uint32_t GetNumElements() const { return m_NumElements; }

        uint32_t GetNumElements(ParamType type) const { return m_NumElementsPerType[(int)type]; }

        uint32_t GetSequentialSlot(ParamType type, uint32_t set, uint32_t slot) const;

        void GetBinding(ParamType type, uint32_t seqSlot, uint32_t& set, uint32_t& slot) const;

        void GetBinding(ShaderType shaderType, ParamType type, const String& name, UniformBinding& binding);

        void GetBinding(ParamType type, const String& name, UniformBinding (&bindings)[SHADER_COUNT]);

        const Ref<UniformDesc>& GetUniformDesc(ShaderType type) const { return m_ParamDescs[(int)type]; }
        static Ref<UniformParamInfo> Create(const UniformParamDesc& desc);

    protected:
        struct SetInfo
        {
            uint32_t* SlotIndices;
            ParamType* SlotTypes;
            uint32_t* SlotSamplers;
            uint32_t NumSlots;
        };

        struct ResourceInfo
        {
            uint32_t Set;
            uint32_t Slot;
        };

        std::array<Ref<UniformDesc>, SHADER_COUNT> m_ParamDescs;

        uint32_t m_NumSets;
        uint32_t m_NumElements;
        SetInfo* m_SetInfos;
        ResourceInfo* m_ResourceInfos[(int)ParamType::Count];
        uint32_t m_NumElementsPerType[(int)ParamType::Count];
    };

} // namespace Crowny