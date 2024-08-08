#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetHandle.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/GpuBuffer.h"
#include "Crowny/RenderAPI/SamplerState.h"

namespace Crowny
{
    class MaterialParamStructData
    {
    public:
        uint8_t* Data;
        uint32_t Size;
    };

    class MaterialParamTextureData
    {
        AssetHandle<Crowny::Texture> Texture;
        bool IsLoadStore;
        TextureSurface Surface;
    };

    class MaterialParams
    {
    public:
        enum class ParamType {
            Data,
            Texture,
            Sampler,
            Buffer
        };
        struct ParamData {
            ParamType Type;
            ParamDataType DataType;

        };

        MaterialParams(const AssetHandle<Shader>& shader, uint64_t initialVersion = 0);
        uint32_t m_LastSync = 0;
    private:
        MaterialParamStructData* m_StructParams = nullptr;
        MaterialParamTextureData* m_TextureParams = nullptr;
        Ref<GpuBuffer>* m_Buffer = nullptr;
        Ref<Texture>* m_DefaultTextureParams = nullptr;
        Ref<SamplerState> m_DefaultSamplers = nullptr;
    };

    template<int DataType>
    class MaterialData
    {
    public:
        MaterialData() = default;`
        MaterialData(const String& name, const Material* material)
            : m_Index(0), m_ArraySize(0), m_Material(nullptr)
        {
            if (material != nullptr) {
                const Ref<MaterialParameters> params = material->GetParameters();
                uint32_t paramIndex = 0;
                auto result = params->GetParamIndex(name, MaterialParams::ParamType::Data, (GpuParamDataType)DataType,
                                                    0, paramIndex);
                if (result == MaterialParams::GetParamResult::Success)
                {
                    const MaterialParams::ParamData* data = params->GetParamData(paramIndex);
                    m_Material = material;
                    m_Index = paramIndex;
                    m_ArraySize = data->ArraySize;
                }
                else
                    params->OnGetParamError(result, name, 0);
            }
        }
        bool operator==(const std::nullptr_t& null) const { return m_Material== null; }
    protected:
        uint32_t m_Index;
        uint32_t m_ArraySize;
        Material* m_Material;
    };
    class MaterialBase
    {
    public:
        MaterialBase() = default;
        virtual ~MaterialBase() = default;
        AssetHandle<Shader> GetShader() const { return m_Shader; }
        void SetFloat(const String& name, float value, uint32_t arrayIdx = 0)
        {
            return GetParamFloat(name).Set(value, arrayIdx);
        }

        // void SetFloatCurve(const String& name, )
        void SetColor(const String& name, const Color& value, uint32_t arrayIdx = 0)
        {
            return GetParamColor(name).Set(value, arrayIdx);
        }

        void SetVector2(const String& name, const glm::vec2& value, uint32_t arrayIdx = 0)
        {
            return GetParamVector2(name).Set(value, arrayIdx);
        }
    };

    class Material : public Asset, public MaterialBase, public AssetListener
    {
    public:
        Material(const AssetHandle<Shader>& shader);
        ~Material() = default;

        // void Bind(uint32_t startslot);

        void SetShader(const AssetHandle<Shader>& shader);
        AssetHandle<Shader> GetShader() { return m_Shader; }

    protected:
        Vector<Ref<Texture>> m_Textures;
        AssetHandle<Shader> m_Shader;

    private:
        friend class MaterialInstance;
    };

    class MaterialInstance
    {
    public:
        MaterialInstance(const Ref<Material>& material);
        const Ref<Material>& GetMaterial() const { return m_Material; }

        // void Bind(uint32_t startslot);
        void SetUniformData(const String& name, byte* data);
        void SetTexture(const String& name, const Ref<Texture>& texture);

    private:
        Vector<Ref<Texture>> m_Textures;
        Ref<Material> m_Material;
    };
} // namespace Crowny
