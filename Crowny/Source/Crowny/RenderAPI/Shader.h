#pragma once

namespace Crowny
{

    struct BinaryShaderData;

    struct UniformBufferBlockDesc
    {
        String Name;
        uint32_t Slot;
        uint32_t Set;
        uint32_t BlockSize;
    };

    struct UniformResourceDesc
    {
        String Name;
        UniformResourceType Type;
        uint32_t Slot;
        uint32_t Set;
        GpuBufferFormat ElementType = BF_UNKNOWN;
    };

    struct UniformDesc
    {
        UnorderedMap<String, UniformBufferBlockDesc> Uniforms;

        UnorderedMap<String, UniformResourceDesc> Samplers;
        UnorderedMap<String, UniformResourceDesc> Textures;
        UnorderedMap<String, UniformResourceDesc> LoadStoreTextures;
    };

    struct ShaderStageDesc
    {
    };

    class ShaderStage
    {
    public:
        virtual const Ref<UniformDesc>& GetUniformDesc() const = 0;
        static Ref<ShaderStage> Create(const Ref<BinaryShaderData>& shaderData);
    };

    struct ShaderDesc
    {
        Ref<BinaryShaderData> VertexShader;
        Ref<BinaryShaderData> FragmentShader;
        Ref<BinaryShaderData> GeometryShader;
        Ref<BinaryShaderData> HullShader;
        Ref<BinaryShaderData> DomainShader;
        Ref<BinaryShaderData> ComputeShader;
    };

    class Shader
    {
    public:
        static Ref<Shader> Create(const ShaderDesc& stateDesc);

    private:
        Array<Ref<ShaderStage>, SHADER_COUNT> m_ShaderStages;
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