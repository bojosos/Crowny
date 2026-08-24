#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

#include <variant>

namespace Crowny
{
    using MaterialPropertyValue =
      std::variant<bool, int32_t, float, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4, AssetHandle<Texture>>;

    class MaterialPropertyBlock
    {
    public:
        template <typename T> void Set(MaterialPropertyID property, const T& value)
        {
            if (!property.IsValid())
                return;
            m_Values.insert_or_assign(property.Value, MaterialPropertyValue(value));
            m_Revision++;
        }

        template <typename T> void Set(StringView property, const T& value) { Set(Shader::PropertyToID(property), value); }

        template <typename T> const T* Get(MaterialPropertyID property) const
        {
            const auto value = m_Values.find(property.Value);
            return value != m_Values.end() ? std::get_if<T>(&value->second) : nullptr;
        }

        bool Has(MaterialPropertyID property) const { return m_Values.find(property.Value) != m_Values.end(); }
        bool Remove(MaterialPropertyID property);
        void Clear();
        bool IsEmpty() const { return m_Values.empty(); }
        uint32_t GetRevision() const { return m_Revision; }
        const UnorderedMap<uint32_t, MaterialPropertyValue>& GetValues() const { return m_Values; }

    private:
        UnorderedMap<uint32_t, MaterialPropertyValue> m_Values;
        uint32_t m_Revision = 0;
    };

} // namespace Crowny
