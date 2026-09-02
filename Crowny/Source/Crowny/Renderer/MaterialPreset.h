#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/Material.h"

#include <glm/glm.hpp>

namespace Crowny
{
    enum class MaterialPresetValueType : uint8_t
    {
        Float,
        Float2,
        Float3,
        Color, // vec4
        Int,
        Bool
    };

    StringView MaterialPresetValueTypeName(MaterialPresetValueType type);
    bool ParseMaterialPresetValueType(StringView name, MaterialPresetValueType& outType);
    ShaderDataType MaterialPresetValueTypeToShaderDataType(MaterialPresetValueType type);

    struct MaterialPresetParameter
    {
        String Name;
        MaterialPresetValueType Type = MaterialPresetValueType::Float;
        glm::vec4 Vector{ 0.0f }; // Float uses x, Float2 xy, Float3 xyz, Color xyzw
        int32_t Integer = 0;      // Int and Bool
    };

    /**
     * A data-driven set of material parameter values. Presets are authored as
     * `.cwpreset` YAML files (built-ins ship under Resources/Presets, users keep
     * theirs in the project) and applied to any material whose shader exposes
     * every parameter with a matching type. Texture assignments are never part of
     * a preset so applying one preserves the artist's texture setup.
     */
    class MaterialPreset : public Asset
    {
    public:
        MaterialPreset() = default;

        virtual AssetType GetAssetType() const override { return AssetType::MaterialPreset; }
        static AssetType GetStaticType() { return AssetType::MaterialPreset; }

        /** Material model ("Toon", "Standard", "Unlit") or shader name this preset targets. Empty matches any material. */
        const String& GetTarget() const { return m_Target; }
        void SetTarget(const String& target) { m_Target = target; }

        const Vector<MaterialPresetParameter>& GetParameters() const { return m_Parameters; }
        const MaterialPresetParameter* Find(StringView name) const;
        bool Remove(StringView name);
        void Clear() { m_Parameters.clear(); }

        void SetFloat(const String& name, float value);
        void SetFloat2(const String& name, const glm::vec2& value);
        void SetFloat3(const String& name, const glm::vec3& value);
        void SetColor(const String& name, const glm::vec4& value);
        void SetInt(const String& name, int32_t value);
        void SetBool(const String& name, bool value);

        /** True when every parameter exists in the layout with a matching data type. */
        bool Validate(const Material::BindingMap& bindings, String* outError = nullptr) const;

        /** True when the target matches the material's classified model or shader name. */
        bool IsCompatibleWith(const Material& material) const;
        bool IsCompatibleWith(MaterialModel model, StringView shaderName) const;

        static StringView MaterialModelName(MaterialModel model);

        /** Snapshots every user-editable data parameter of a material. */
        static Ref<MaterialPreset> CaptureFromMaterial(const Material& material, const String& name);

    private:
        MaterialPresetParameter& Upsert(const String& name, MaterialPresetValueType type);

        CW_SERIALIZABLE(MaterialPreset);

        String m_Target;
        Vector<MaterialPresetParameter> m_Parameters;
    };
} // namespace Crowny
