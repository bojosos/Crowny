#pragma once

#include "Crowny/Assets/Asset.h"

#include <cereal/types/polymorphic.hpp>

namespace Crowny
{

    struct BinaryShaderData;

    struct ShaderDefines
    {
        Vector<String> SingularDefines;
        UnorderedMap<String, String> ValueDefines;
    };

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

    class Shader;

    class ShaderStage
    {
    public:
        ShaderStage() = default;
        ShaderStage(const Ref<BinaryShaderData>& shaderData);
        const Ref<UniformDesc>& GetUniformDesc() const;
        static Ref<ShaderStage> Create(const Ref<BinaryShaderData>& shaderData);

    protected:
        CW_SERIALIZABLE(Shader);
        Ref<BinaryShaderData> m_ShaderData;
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

    class Shader : public Asset
    {
    public:
        Shader() = default;

        static Ref<Shader> Create(const ShaderDesc& stateDesc);
        Ref<ShaderStage> GetStage(ShaderType shaderType) const { return m_ShaderStages[shaderType]; }

        virtual AssetType GetAssetType() const override { return AssetType::Shader; }
		static AssetType GetStaticType() { return AssetType::Shader; }

    private:
        CW_SERIALIZABLE(Shader);
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
