#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/Renderer/ShaderParameter.h"
#include "Crowny/Renderer/ShaderVariation.h"

#include <cereal/types/polymorphic.hpp>

namespace Crowny
{

    /*
        Final goal:
        RenderPassDesc passDesc;
        passDesc.VertexShader = vert;
        passDesc.FragmentShader = frag;
        passDesc.BlendState = blendState;

        RenderPass passDesc(desc);

        Technique technique(passDesc);

        ShaderDesc shaderDesc;
        shaderDesc.Techniques = { technique };
        Shader shader(static_variants, dynamic_variants, shaderDesc);
    */

    struct UniformDesc;
    struct BlendStateDesc;
    struct RasterizerStateDesc;
    struct DepthStencilStateDesc;

    enum class QueuePriority
    {
        Opaque = 1000,
        Transparent = 900,
        Skybox = 800,
        Overlay = 700
    };

    enum class QueueSortType
    {
        FrontToBack,
        BackToFront,
        None,
    };

    struct BinaryShaderData : public RefCounted // TODO: Think of a better name
    {
        Vector<uint8_t> Data;
        String EntryPoint;
        ShaderType Type = ShaderType::VERTEX_SHADER;
        Ref<UniformDesc> Description;
        BufferLayout VertexLayout;

        BinaryShaderData() = default;
        BinaryShaderData(const Vector<uint8_t>& data, const String& entryPoint, ShaderType type, const Ref<UniformDesc>& uniformDesc)
          : Data(data), EntryPoint(entryPoint), Type(type), Description(uniformDesc)
        {
        }
    };

    struct ShaderRenderPassDesc
    {
        Ref<BlendStateDesc> BlendState;
        Ref<RasterizerStateDesc> RasterizationState;
        Ref<DepthStencilStateDesc> DepthStencilState;
        uint32_t StencilValue;

        Ref<BinaryShaderData> VertexShader;
        Ref<BinaryShaderData> FragmentShader;
        Ref<BinaryShaderData> GeometryShader;
        Ref<BinaryShaderData> HullShader;
        Ref<BinaryShaderData> DomainShader;
        Ref<BinaryShaderData> ComputeShader;

        Ref<BinaryShaderData> RaygenShader;
        Ref<BinaryShaderData> MissShader;
        Ref<BinaryShaderData> HitShader;
    };

    class ShaderTechnique;

    struct ShaderDesc
    {
        int32_t ShaderQueuePriority;
        QueueSortType QueueSort;
        Vector<Ref<ShaderTechnique>> Techniques;
    };

    class ShaderRenderPass : public RefCounted
    {
    public:
        ShaderRenderPass() = default;
        static Ref<ShaderRenderPass> Create(const ShaderRenderPassDesc& shaderDesc);

        void Compile();
        bool IsCompute() const { return m_ShaderDesc.ComputeShader != nullptr; }
        bool IsRayTrace() const { return m_ShaderDesc.RaygenShader != nullptr; }
        bool HasBlending() const;

        const Ref<GraphicsPipeline>& GetGraphicsPipeline() const { return m_GraphicsPipeline; }
        const Ref<RayTracingPipeline>& GetRayTracingPipeline() const { return m_RayTracingPipeline; }
        const Ref<ComputePipeline>& GetComputePipeline() const { return m_ComputePipeline; }

        const ShaderRenderPassDesc& GetPassDesc() const { return m_ShaderDesc; }

    protected:
        ShaderRenderPass(const ShaderRenderPassDesc& shaderDescription);

    private:
        CW_SIMPLESERIALIZABLE(ShaderRenderPass);
        ShaderRenderPassDesc m_ShaderDesc;
        Ref<GraphicsPipeline> m_GraphicsPipeline;
        Ref<RayTracingPipeline> m_RayTracingPipeline;
        Ref<ComputePipeline> m_ComputePipeline;
    };

    class ShaderTechnique : public RefCounted
    {
    public:
        ShaderTechnique() = default;

        static Ref<ShaderTechnique> Create(const Vector<String>& tags, const ShaderVariation& variation,
                                           const Vector<Ref<ShaderRenderPass>>& renderPasses);
        void Compile();
        const Vector<Ref<ShaderRenderPass>>& GetRenderPasses() const { return m_Passes; }
        const ShaderVariation& GetVariation() const { return m_Variation; }

    protected:
        ShaderTechnique(const Vector<String>& tags, const ShaderVariation& variation, const Vector<Ref<ShaderRenderPass>>& renderPasses);

    private:
        CW_SIMPLESERIALIZABLE(ShaderTechnique);
        Vector<String> m_Tags;
        ShaderVariation m_Variation;
        Vector<Ref<ShaderRenderPass>> m_Passes;
    };

    struct BinaryShaderData;

    struct UniformBufferBlockMember
    {
        String Name;
        uint32_t Offset;
        ShaderDataType DataType;
        Vector<uint8_t> DefaultValue; // Raw bytes parsed from @default annotation (empty = no default)

        template <typename Archive> void Serialize(Archive& archive) { archive(Offset, DataType, Name, DefaultValue); }
    };

    struct UniformBufferBlockDesc
    {
        String Name; // Maybe remove the names? It's already a map with key name?
        Vector<UniformBufferBlockMember> Members;
        uint32_t Slot;
        uint32_t Set;
        uint32_t BlockSize;

        template <typename Archive> void Serialize(Archive& archive) { archive(Name, Slot, Set, BlockSize, Members); }
    };

    struct UniformResourceDesc
    {
        String Name; // Maybe remove the names? It's already a map with key name?
        UniformResourceType Type;
        uint32_t Slot;
        uint32_t Set;
        GpuBufferFormat ElementType = BF_UNKNOWN;

        template <typename Archive> void Serialize(Archive& archive) { archive(Name, Type, Slot, Set, ElementType); }
    };

    struct AccelerationStructDesc
    {
        String Name; // Maybe remove the names? It's already a map with key name?
        uint32_t Slot;
        uint32_t Set;

        template <typename Archive> void Serialize(Archive& archive) { archive(Name, Slot, Set); }
    };

    struct UniformDesc : public RefCounted
    {
        UnorderedMap<String, UniformBufferBlockDesc> Uniforms;

        UnorderedMap<String, UniformResourceDesc> Samplers;
        UnorderedMap<String, UniformResourceDesc> Textures;
        UnorderedMap<String, UniformResourceDesc> Buffers;
        UnorderedMap<String, UniformResourceDesc> LoadStoreTextures;
        UnorderedMap<String, AccelerationStructDesc> AccelerationStructures;

        // Per-parameter annotations parsed from GLSL comments (// @color, // @range, etc.)
        UnorderedMap<String, AnnotationSet> Annotations;
    };

    class ShaderStage : public RefCounted
    {
    public:
        ShaderStage() = default;
        const Ref<UniformDesc>& GetUniformDesc() const;
        static Ref<ShaderStage> Create(const Ref<BinaryShaderData>& shaderData);
        Ref<BufferLayout> GetBufferLayout() const { return m_BufferLayout; }

    protected:
        ShaderStage(const Ref<BinaryShaderData>& shaderData);
        CW_SERIALIZABLE(ShaderStage);
        Ref<BufferLayout> m_BufferLayout;
        Ref<BinaryShaderData> m_ShaderData; // TODO: Don't store the binary data here.
    };

    class Shader : public Asset
    {
    public:
        Shader() = default;

        static Ref<Shader> Create(const ShaderDesc& stateDesc);
        virtual AssetType GetAssetType() const override { return AssetType::Shader; }
        static AssetType GetStaticType() { return AssetType::Shader; }
        const Vector<Ref<ShaderTechnique>>& GetTechniques() const { return m_Techniques; }

        // Find the technique whose variation matches. Falls back to the first technique.
        const Ref<ShaderTechnique>& GetTechnique(const ShaderVariation& variation) const
        {
            CW_ENGINE_ASSERT(!m_Techniques.empty(), "Shader has no techniques");
            for (const auto& tech : m_Techniques)
            {
                if (tech->GetVariation().Matches(variation))
                    return tech;
            }
            return m_Techniques[0]; // Fallback to base variant
        }

    protected:
        Shader(const ShaderDesc& shaderDesc);

    private:
        CW_SERIALIZABLE(Shader);
        Vector<Ref<ShaderTechnique>> m_Techniques;
    };

    class ShaderLibrary
    {
    public:
        void Add(const String& name, const Ref<Shader>& shader);
        void Add(const Ref<Shader>& shader);
        Ref<Shader> Load(const Path& filepath);
        Ref<Shader> Load(const String& name, const Path& filepath);

        Ref<Shader> Get(const String& name);

        bool Exists(const String& name) const;

    private:
        String m_Name;
        UnorderedMap<String, Ref<Shader>> m_Shaders;
    };
} // namespace Crowny
