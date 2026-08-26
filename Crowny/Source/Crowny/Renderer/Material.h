#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetListener.h"

#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/StringID.h"

#include "Crowny/RenderAPI/GpuBuffer.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/ShaderParameter.h"

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"

namespace Crowny
{
    class Material;

    // Cached handle to a data parameter in a Material. Avoids repeated string lookups.
    // Obtain via Material::GetParam<T>(name). Invalidated when the Material's shader changes.
    template <typename T> class MaterialParamHandle
    {
    public:
        MaterialParamHandle() = default;

        void Set(const T& value);
        T Get() const;
        bool IsValid() const { return m_Material != nullptr; }

    private:
        friend class Material;
        MaterialParamHandle(Material* mat, uint32_t offset, StringID bufferID) : m_Material(mat), m_Offset(offset), m_BufferID(bufferID) {}

        Material* m_Material = nullptr;
        uint32_t m_Offset = 0;
        StringID m_BufferID;
    };

    // Cached handle to a texture parameter in a Material.
    class MaterialTextureHandle
    {
    public:
        MaterialTextureHandle() = default;

        void Set(const AssetHandle<Texture>& tex);
        void Set(const Ref<Texture>& tex);
        AssetHandle<Texture> Get() const;
        bool IsValid() const { return m_Material != nullptr; }

    private:
        friend class Material;
        MaterialTextureHandle(Material* mat, const String& name) : m_Material(mat), m_Name(name) {}

        Material* m_Material = nullptr;
        String m_Name;
    };

    class Material : public Asset, public AssetListener
    {
    public:
        // A map from uniform buffer member name to uniform buffer name.
        struct UniformMember
        {
            uint32_t Offset;
            ShaderDataType DataType;
            String BufferName;
            StringID BufferID;
        };

        using BindingMap = UnorderedMap<String, UniformMember, StringHash, StringEqual>;

        struct PassData
        {
            Ref<GraphicsPipeline> Pipeline;
            Ref<UniformParams> Uniforms;
            UnorderedMap<StringID, Ref<UniformBufferBlock>> UniformBlocks;
        };

        Material(const AssetHandle<Shader>& shader);
        virtual ~Material() override = default;

        virtual AssetType GetAssetType() const override { return AssetType::Material; }
        static AssetType GetStaticType() { return AssetType::Material; }

        static Ref<Material> Create(const AssetHandle<Shader>& shader);
        static Ref<Material> CreatePBR(const AssetHandle<Shader>& shader);
        static Ref<Material> CreateToon(const AssetHandle<Shader>& shader);
        static Ref<Material> CreateUnlit(const AssetHandle<Shader>& shader);

        AssetHandle<Shader> GetShader() const { return m_Shader; }
        virtual void GetAssets(Vector<AssetHandle<Asset>>& assets) override { assets.push_back(m_Shader); }

        void SetShader(const AssetHandle<Shader>& shader);
        void SetVariation(const ShaderVariation& variation);
        const ShaderVariation& GetVariation() const { return m_Variation; }
        void ReloadParams();

        uint64_t GetParamVersion() const { return m_ParamVersion; }
        /** Globally unique generation for the current reflected parameter layout. */
        uint64_t GetLayoutVersion() const { return m_LayoutVersion; }

        // Typed parameter handles — caches the lookup, avoids string search on every Set/Get.
        template <typename T> MaterialParamHandle<T> GetParam(const String& name)
        {
            auto it = m_Bindings.find(name);
            if (it == m_Bindings.end())
                return {};
            if (it->second.DataType != ShaderDataTypeTrait<T>::Type)
                return {};
            return MaterialParamHandle<T>(this, it->second.Offset, it->second.BufferID);
        }

        MaterialTextureHandle GetTextureParam(const String& name) { return MaterialTextureHandle(this, name); }

        const BindingMap& GetBindings() const { return m_Bindings; }
        bool HasBinding(const String& name) const { return m_Bindings.find(name) != m_Bindings.cend(); }
        bool HasBinding(HashedString name) const { return m_Bindings.find(name) != m_Bindings.cend(); }

        template <typename T> T GetDataParam(const String& name) const
        {
            const auto iterFind = m_Bindings.find(name);
            if (iterFind == m_Bindings.cend())
            {
                CW_ENGINE_WARN("Could not find uniform {}", name);
                return T();
            }
            constexpr ShaderDataType expectedType = ShaderDataTypeTrait<T>::Type;
            if (iterFind->second.DataType != expectedType)
            {
                CW_ENGINE_WARN("Type mismatch for uniform {}: expected {}, got {}", name, ShaderDataTypeToString(expectedType),
                               ShaderDataTypeToString(iterFind->second.DataType));
                return T();
            }
            T value;
            // Read from first pass that has it
            for (const auto& pass : m_Passes)
            {
                const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
                if (blockIt != pass.UniformBlocks.end())
                {
                    blockIt->second->Read(iterFind->second.Offset, &value, sizeof(value));
                    return value;
                }
            }
            return T();
        }

        Ref<Texture> GetTexture(uint32_t set, uint32_t slot) const { return m_Passes[0].Uniforms->GetTexture(set, slot); }
        UniformDesc::TextureMap GetTextures() const { return GetTextureDescriptors(); }

        /** Returns the reflected texture layout. The reference is valid until the next ReloadParams(). */
        const UniformDesc::TextureMap& GetTextureDescriptors() const
        {
            static const UniformDesc::TextureMap s_Empty;
            if (m_Passes.empty() || m_Passes[0].Pipeline == nullptr)
                return s_Empty;
            const Ref<UniformDesc>& desc = m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc(FRAGMENT_SHADER);
            return desc != nullptr ? desc->Textures : s_Empty;
        }

        const UnorderedMap<String, AnnotationSet>& GetAnnotations(ShaderType shaderType = FRAGMENT_SHADER) const
        {
            static const UnorderedMap<String, AnnotationSet> s_Empty;
            const Ref<UniformDesc>& desc = m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc(shaderType);
            return desc ? desc->Annotations : s_Empty;
        }

        // Get the binding slot for a uniform buffer block by name (for sort ordering)
        uint32_t GetBlockBindingSlot(const String& blockName) const
        {
            for (uint32_t i = 0; i < SHADER_COUNT; i++)
            {
                const Ref<UniformDesc>& desc = m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc((ShaderType)i);
                if (!desc)
                    continue;
                auto it = desc->Uniforms.find(blockName);
                if (it != desc->Uniforms.end())
                    return it->second.Slot;
            }
            return 0;
        }

        void FlushUniformBuffers();
        void SetBool(const String& name, bool value);
        void SetFloat(const String& name, float value);
        void SetFloat(HashedString name, float value);
        void SetFloat(MaterialPropertyID name, float value);
        void SetFloat2(const String& name, const glm::vec2& value);
        void SetFloat3(const String& name, const glm::vec3& value);
        void SetInt(const String& name, int value);
        void SetInt(HashedString name, int value);
        void SetInt(MaterialPropertyID name, int value);
        void SetInt2(const String& name, const glm::ivec2& value);
        void SetInt3(const String& name, const glm::ivec3& value);
        void SetInt4(const String& name, const glm::ivec4& value);
        void SetColor(const String& name, const glm::vec4& color);
        void SetColor(HashedString name, const glm::vec4& color);
        void SetColor(MaterialPropertyID name, const glm::vec4& color);
        void SetVector4Array(const String& name, const glm::vec4* values, uint32_t count);
        void SetInt4Array(const String& name, const glm::ivec4* values, uint32_t count);
        void SetVector3(const String& name, const glm::vec3& value);
        void SetVector3(HashedString name, const glm::vec3& value);
        void SetVector3(MaterialPropertyID name, const glm::vec3& value);
        void SetMat3(const String& name, const glm::mat3& value);
        void SetMatrix(const String& name, const glm::mat4& matrix);
        void SetMatrix(HashedString name, const glm::mat4& matrix);
        void SetMatrix(MaterialPropertyID name, const glm::mat4& matrix);
        void SetTexture(const String& name, const AssetHandle<Texture>& texture);
        void SetTexture(const String& name, const Ref<Texture>& texture);
        void SetTexture(HashedString name, const Ref<Texture>& texture);
        void SetTexture(MaterialPropertyID name, const Ref<Texture>& texture);

        AssetHandle<Texture> GetTextureHandle(const String& name) const
        {
            auto it = m_TextureHandles.find(name);
            return (it != m_TextureHandles.end()) ? it->second : AssetHandle<Texture>();
        }

        // Multi-pass accessors
        uint32_t GetPassCount() const { return (uint32_t)m_Passes.size(); }
        const Ref<UniformParams>& GetUniformParams(uint32_t pass = 0) const { return m_Passes[pass].Uniforms; }
        const Ref<GraphicsPipeline>& GetGraphicsPipeline(uint32_t pass = 0) const { return m_Passes[pass].Pipeline; }

    private:
        template <typename T> friend class MaterialParamHandle;
        friend class MaterialTextureHandle;
        friend class cereal::access;
        Material() = default; // For serialization only
        void CreateAndAppendUniforms(uint32_t passIndex);
        void ApplyDefaults();
        static uint64_t NextLayoutVersion();
        template <typename Name, typename Value>
        void SetDataParam(const Name& name, ShaderDataType expectedType, const Value& value, StringView valueType);

    private:
        CW_SERIALIZABLE(Material);

        Vector<PassData> m_Passes;
        BindingMap m_Bindings;
        UnorderedMap<String, AssetHandle<Texture>> m_TextureHandles;
        AssetHandle<Shader> m_Shader;
        ShaderVariation m_Variation;
        uint64_t m_ParamVersion = 0;
        uint64_t m_LayoutVersion = NextLayoutVersion();
    };
} // namespace Crowny
