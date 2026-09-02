#include "cwepch.h"

#include "Panels/MaterialInspectorSchemaCache.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Renderer/Material.h"

#include <algorithm>
#include <cctype>

namespace Crowny
{
    namespace
    {
        class RuntimeMaterialSchemaSource final : public MaterialInspectorSchemaSource
        {
        public:
            explicit RuntimeMaterialSchemaSource(const Material& material) : m_Material(material) {}

            uint64_t GetLayoutVersion() const override { return m_Material.GetLayoutVersion(); }
            const Material::BindingMap& GetBindings() const override { return m_Material.GetBindings(); }
            const UniformDesc::TextureMap& GetTextureDescriptors() const override { return m_Material.GetTextureDescriptors(); }
            const UnorderedMap<String, AnnotationSet>& GetAnnotations(ShaderType shaderType) const override
            {
                return m_Material.GetAnnotations(shaderType);
            }
            uint32_t GetBlockBindingSlot(const String& blockName) const override { return m_Material.GetBlockBindingSlot(blockName); }

        private:
            const Material& m_Material;
        };

        String AutoDisplayName(const String& identifier)
        {
            String result;
            result.reserve(identifier.size() + 4u);
            for (size_t index = 0; index < identifier.size(); index++)
            {
                const unsigned char value = static_cast<unsigned char>(identifier[index]);
                const char character = static_cast<char>(value);
                if (index == 0)
                    result += static_cast<char>(std::toupper(value));
                else if (std::isupper(value))
                {
                    result += ' ';
                    result += character;
                }
                else if (character == '_')
                    result += ' ';
                else
                    result += character;
            }
            return result;
        }

        ShaderParamType MapDataType(ShaderDataType dataType, bool isColor)
        {
            switch (dataType)
            {
            case ShaderDataType::Float:
                return ShaderParamType::Float;
            case ShaderDataType::Float2:
                return ShaderParamType::Float2;
            case ShaderDataType::Float3:
                return isColor ? ShaderParamType::Color3 : ShaderParamType::Float3;
            case ShaderDataType::Float4:
                return isColor ? ShaderParamType::Color4 : ShaderParamType::Float4;
            case ShaderDataType::Int:
                return ShaderParamType::Int;
            case ShaderDataType::Int2:
                return ShaderParamType::Int2;
            case ShaderDataType::Int3:
                return ShaderParamType::Int3;
            case ShaderDataType::Int4:
                return ShaderParamType::Int4;
            case ShaderDataType::Bool:
                return ShaderParamType::Bool;
            case ShaderDataType::Mat3:
                return ShaderParamType::Mat3;
            case ShaderDataType::Mat4:
                return ShaderParamType::Mat4;
            default:
                return ShaderParamType::Float;
            }
        }

        ShaderParamType MapResourceType(UniformResourceType resourceType)
        {
            switch (resourceType)
            {
            case SAMPLER2D:
            case TEXTURE2D:
                return ShaderParamType::Texture2D;
            case SAMPLER3D:
            case TEXTURE3D:
                return ShaderParamType::Texture3D;
            case SAMPLERCUBE:
            case TEXTURECUBE:
                return ShaderParamType::TextureCube;
            default:
                return ShaderParamType::Texture2D;
            }
        }

        const AnnotationSet* FindAnnotation(const MaterialInspectorSchemaSource& source, const String& name)
        {
            const AnnotationSet* annotation = nullptr;
            for (uint32_t stage = 0; stage < SHADER_COUNT; stage++)
            {
                const auto& annotations = source.GetAnnotations(static_cast<ShaderType>(stage));
                const auto entry = annotations.find(name);
                if (entry != annotations.end())
                    annotation = &entry->second;
            }
            return annotation;
        }
    } // namespace

    Vector<const MaterialShaderOption*> FilterMaterialShaderOptions(const Vector<MaterialShaderOption>& options, StringView search,
                                                                    bool includeInternal)
    {
        Vector<const MaterialShaderOption*> result;
        result.reserve(options.size());
        for (const MaterialShaderOption& option : options)
        {
            if (option.BuiltIn && !option.MaterialCapable && !includeInternal)
                continue;
            if (!search.empty() && !StringUtils::IsSearchMathing(option.Name, search))
                continue;
            result.push_back(&option);
        }
        std::stable_sort(result.begin(), result.end(), [](const MaterialShaderOption* left, const MaterialShaderOption* right) {
            if (left->BuiltIn != right->BuiltIn)
                return left->BuiltIn;
            if (left->BuiltIn && left->MaterialCapable != right->MaterialCapable)
                return left->MaterialCapable;
            return StringUtils::CaseInsensitiveCompare(left->Name, right->Name);
        });
        return result;
    }

    const Vector<ShaderParameterDesc>& MaterialInspectorSchemaCache::Resolve(const Material& material)
    {
        const RuntimeMaterialSchemaSource source(material);
        return Resolve(source);
    }

    const Vector<ShaderParameterDesc>& MaterialInspectorSchemaCache::Resolve(const MaterialInspectorSchemaSource& source)
    {
        const uint64_t layoutVersion = source.GetLayoutVersion();
        if (m_HasLayout && m_LayoutVersion == layoutVersion)
            return m_Parameters;

        Vector<ShaderParameterDesc> parameters = Build(source);
        m_Parameters = std::move(parameters);
        m_LayoutVersion = layoutVersion;
        m_HasLayout = true;
        return m_Parameters;
    }

    void MaterialInspectorSchemaCache::Reset()
    {
        m_LayoutVersion = 0;
        m_HasLayout = false;
        m_Parameters.clear();
    }

    Vector<ShaderParameterDesc> MaterialInspectorSchemaCache::Build(const MaterialInspectorSchemaSource& source)
    {
        const Material::BindingMap& bindings = source.GetBindings();
        const UniformDesc::TextureMap& textures = source.GetTextureDescriptors();
        Vector<ShaderParameterDesc> parameters;
        parameters.reserve(bindings.size() + textures.size());

        for (const auto& [name, member] : bindings)
        {
            if (member.BufferName.rfind("cw_", 0) == 0)
                continue;

            ShaderParameterDesc parameter;
            parameter.Identifier = name;
            parameter.BlockName = member.BufferName;
            parameter.Offset = member.Offset;

            const AnnotationSet* annotation = FindAnnotation(source, name);
            bool isColor = false;
            if (annotation != nullptr)
            {
                if (annotation->IsHidden)
                    continue;
                parameter.DisplayName = annotation->DisplayName;
                isColor = annotation->IsColor;
                if (annotation->IsHDR)
                    parameter.Flags.Set(ShaderParamFlag::HDR);
                if (annotation->HasRange)
                {
                    parameter.RangeMin = annotation->RangeMin;
                    parameter.RangeMax = annotation->RangeMax;
                    parameter.HasRange = true;
                }
            }

            parameter.Type = MapDataType(member.DataType, isColor);
            if (parameter.DisplayName.empty())
                parameter.DisplayName = AutoDisplayName(name);
            parameter.SortOrder = source.GetBlockBindingSlot(member.BufferName) * 1000u + member.Offset;
            parameters.push_back(std::move(parameter));
        }

        for (const auto& [name, resource] : textures)
        {
            if (name.rfind("cw_", 0) == 0)
                continue;

            ShaderParameterDesc parameter;
            parameter.Identifier = name;
            parameter.Type = MapResourceType(resource.Type);
            parameter.Set = resource.Set;
            parameter.Slot = resource.Slot;

            const AnnotationSet* annotation = FindAnnotation(source, name);
            if (annotation != nullptr)
            {
                if (annotation->IsHidden)
                    continue;
                parameter.DisplayName = annotation->DisplayName;
            }
            if (parameter.DisplayName.empty())
                parameter.DisplayName = AutoDisplayName(name);

            parameter.SortOrder = 100000u + resource.Slot;
            parameters.push_back(std::move(parameter));
        }

        std::sort(parameters.begin(), parameters.end(),
                  [](const ShaderParameterDesc& left, const ShaderParameterDesc& right) { return left.SortOrder < right.SortOrder; });
        return parameters;
    }
} // namespace Crowny
