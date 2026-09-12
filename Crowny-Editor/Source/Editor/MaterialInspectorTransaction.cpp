#include "cwepch.h"

#include "Editor/MaterialInspectorTransaction.h"

#include <variant>

namespace Crowny
{
    class MaterialInspectorSnapshot : public RefCounted
    {
    public:
        using Value = std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, glm::ivec2, glm::ivec3, glm::ivec4, bool, glm::mat3, glm::mat4>;

        explicit MaterialInspectorSnapshot(const Material& material)
          : m_Shader(material.GetShader()), m_AlphaMode(material.GetAlphaMode()), m_HasAlphaModeOverride(material.HasAlphaModeOverride()),
            m_DecalResponseMask(material.GetDecalResponseMask())
        {
            for (const auto& [name, binding] : material.GetBindings())
            {
                if (binding.BufferName.rfind("cw_", 0) == 0)
                    continue;
                switch (binding.DataType)
                {
                case ShaderDataType::Float:
                    Capture<float>(material, name);
                    break;
                case ShaderDataType::Float2:
                    Capture<glm::vec2>(material, name);
                    break;
                case ShaderDataType::Float3:
                    Capture<glm::vec3>(material, name);
                    break;
                case ShaderDataType::Float4:
                    Capture<glm::vec4>(material, name);
                    break;
                case ShaderDataType::Int:
                    Capture<int>(material, name);
                    break;
                case ShaderDataType::Int2:
                    Capture<glm::ivec2>(material, name);
                    break;
                case ShaderDataType::Int3:
                    Capture<glm::ivec3>(material, name);
                    break;
                case ShaderDataType::Int4:
                    Capture<glm::ivec4>(material, name);
                    break;
                case ShaderDataType::Bool:
                    Capture<bool>(material, name);
                    break;
                case ShaderDataType::Mat3:
                    Capture<glm::mat3>(material, name);
                    break;
                case ShaderDataType::Mat4:
                    Capture<glm::mat4>(material, name);
                    break;
                default:
                    break;
                }
            }
            for (const auto& [name, descriptor] : material.GetTextureDescriptors())
            {
                if (name.rfind("cw_", 0) != 0)
                    m_Textures.emplace(name, TextureValue{ material.GetTextureHandle(name), material.GetTexture(descriptor.Set, descriptor.Slot) });
            }
        }

        bool operator==(const MaterialInspectorSnapshot& other) const
        {
            return m_Shader.GetHandleData() == other.m_Shader.GetHandleData() && m_HasAlphaModeOverride == other.m_HasAlphaModeOverride &&
                   (!m_HasAlphaModeOverride || m_AlphaMode == other.m_AlphaMode) && m_DecalResponseMask == other.m_DecalResponseMask &&
                   m_Values == other.m_Values && m_Textures == other.m_Textures;
        }

        void Apply(Material& material) const
        {
            if (material.GetShader().GetHandleData() != m_Shader.GetHandleData())
                material.SetShader(m_Shader);
            if (m_HasAlphaModeOverride)
                material.SetAlphaMode(m_AlphaMode);
            else
                material.ClearAlphaModeOverride();
            material.SetDecalResponseMask(m_DecalResponseMask);
            for (const auto& [name, value] : m_Values)
            {
                std::visit(
                  [&](const auto& data) {
                      auto parameter = material.GetParam<std::decay_t<decltype(data)>>(name);
                      if (parameter.IsValid())
                          parameter.Set(data);
                  },
                  value);
            }
            for (const auto& [name, texture] : m_Textures)
            {
                if (texture.Handle.HasUUID())
                    material.SetTexture(name, texture.Handle);
                else
                    material.SetTexture(name, texture.Resource);
            }
        }

    private:
        template <typename T> void Capture(const Material& material, const String& name) { m_Values.emplace(name, material.GetDataParam<T>(name)); }

        struct TextureValue
        {
            AssetHandle<Texture> Handle;
            Ref<Texture> Resource;

            bool operator==(const TextureValue& other) const
            {
                return Handle.GetHandleData() == other.Handle.GetHandleData() && Resource == other.Resource;
            }
        };

        AssetHandle<Shader> m_Shader;
        AlphaMode m_AlphaMode;
        bool m_HasAlphaModeOverride;
        uint32_t m_DecalResponseMask;
        Map<String, Value> m_Values;
        Map<String, TextureValue> m_Textures;
    };

    namespace
    {
        class MaterialEditAction final : public UndoAction
        {
        public:
            MaterialEditAction(Path path, Ref<Material> material, Ref<AssetSaveTracker> saveTracker, Ref<MaterialInspectorSnapshot> before,
                               Ref<MaterialInspectorSnapshot> after)
              : UndoAction("Edit material"), m_Path(std::move(path)), m_Material(std::move(material)), m_SaveTracker(std::move(saveTracker)),
                m_Before(std::move(before)), m_After(std::move(after))
            {
            }

            void Commit() override { Apply(*m_After); }
            void Revert() override { Apply(*m_Before); }

        private:
            void Apply(const MaterialInspectorSnapshot& snapshot)
            {
                snapshot.Apply(*m_Material);
                m_SaveTracker->Queue(m_Path, StaticRefCast<Asset>(m_Material));
            }

            Path m_Path;
            Ref<Material> m_Material;
            Ref<AssetSaveTracker> m_SaveTracker;
            Ref<MaterialInspectorSnapshot> m_Before, m_After;
        };
    } // namespace

    void MaterialInspectorTransaction::Capture(const Path& path, const Ref<Material>& material, const Ref<AssetSaveTracker>& saveTracker)
    {
        Reset();
        if (path.empty() || !material || !saveTracker)
            return;
        m_Path = path;
        m_Material = material;
        m_SaveTracker = saveTracker;
    }

    void MaterialInspectorTransaction::BeforeItemInteraction(const UndoItemInteraction& interaction)
    {
        // Widgets edit local values and report interaction before applying them to the material.
        // Capture once at the first edit, without allocating snapshots for idle inspector frames.
        if (!m_Before && m_Material && interaction.Changed)
            m_Before = CreateRef<MaterialInspectorSnapshot>(*m_Material);
    }

    Ref<UndoAction> MaterialInspectorTransaction::Build() const
    {
        if (!m_Before)
            return {};
        auto after = CreateRef<MaterialInspectorSnapshot>(*m_Material);
        if (*m_Before == *after)
            return {};
        return CreateRef<MaterialEditAction>(m_Path, m_Material, m_SaveTracker, m_Before, std::move(after));
    }

    void MaterialInspectorTransaction::Reset()
    {
        m_Path.clear();
        m_Material = nullptr;
        m_SaveTracker = nullptr;
        m_Before = nullptr;
    }
} // namespace Crowny
