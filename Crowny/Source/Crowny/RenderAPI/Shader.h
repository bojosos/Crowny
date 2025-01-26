#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
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

    class UniformDesc;
    class BlendStateDesc;
    class RasterizerStateDesc;
    class DepthStencilStateDesc;

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

    struct BinaryShaderData // TODO: Think of a better name
    {
        Vector<uint8_t> Data;
        String EntryPoint;
        ShaderType Type = ShaderType::VERTEX_SHADER;
        Ref<UniformDesc> Description;
        BufferLayout VertexLayout;

        BinaryShaderData() = default;
        BinaryShaderData(const Vector<uint8_t>& data, const String& entryPoint, ShaderType type,
                         const Ref<UniformDesc>& uniformDesc)
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
    };

    class ShaderTechnique;

    struct ShaderDesc
    {
        int32_t ShaderQueuePriority;
        QueueSortType QueueSort;
        Vector<Ref<ShaderTechnique>> Techniques;
    };

    class ShaderRenderPass
    {
    public:
        void Compile();
        bool IsCompute() { return m_ShaderDesc.ComputeShader != nullptr; }
        bool HasBlending() const;

        const Ref<GraphicsPipeline>& GetGraphicsPipeline() const { return m_GraphicsPipeline; }
        const Ref<ComputePipeline>& GetComputePipeline() const { return m_ComputePipeline; }

        static Ref<ShaderRenderPass> Create(const ShaderRenderPassDesc& shaderDesc);

        ShaderRenderPass(const ShaderRenderPassDesc& shaderDescription);
    private:

        ShaderRenderPassDesc m_ShaderDesc;
        Ref<GraphicsPipeline> m_GraphicsPipeline;
        Ref<ComputePipeline> m_ComputePipeline;
    };

    class ShaderTechnique
    {
    public:
        ShaderTechnique() = default;

        ShaderTechnique(const Vector<String>& tags, const ShaderVariation& variation,
                        const Vector<Ref<ShaderRenderPass>>& renderPasses);

        static Ref<ShaderTechnique> Create(const Vector<String>& tags, const ShaderVariation& variation,
                                    const Vector<Ref<ShaderRenderPass>>& renderPasses);
        void Compile();

    private:
        Vector<String> m_Tags;
        ShaderVariation m_Variation;
        Vector<Ref<ShaderRenderPass>> m_Passes;
    };

    struct BinaryShaderData;

    struct UniformBufferBlockDesc
    {
        String Name;
        uint32_t Slot;
        uint32_t Set;
        uint32_t BlockSize;

        template <typename Archive> void Serialize(Archive& archive) { archive(Name, Slot, Set, BlockSize); }
    };

    struct UniformResourceDesc
    {
        String Name;
        UniformResourceType Type;
        uint32_t Slot;
        uint32_t Set;
        GpuBufferFormat ElementType = BF_UNKNOWN;

        template <typename Archive> void Serialize(Archive& archive) { archive(Name, Type, Slot, Set, ElementType); }
    };

    struct UniformDesc
    {
        UnorderedMap<String, UniformBufferBlockDesc> Uniforms;

        UnorderedMap<String, UniformResourceDesc> Samplers;
        UnorderedMap<String, UniformResourceDesc> Textures;
        UnorderedMap<String, UniformResourceDesc> LoadStoreTextures;
    };

    class ShaderStage
    {
    public:
        ShaderStage() = default;
        ShaderStage(const Ref<BinaryShaderData>& shaderData);
        const Ref<UniformDesc>& GetUniformDesc() const;
        static Ref<ShaderStage> Create(const Ref<BinaryShaderData>& shaderData);
        Ref<BufferLayout> GetBufferLayout() const { return m_BufferLayout; }
    protected:
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
