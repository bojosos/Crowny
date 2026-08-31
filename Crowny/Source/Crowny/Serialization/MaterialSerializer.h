#pragma once

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    class MaterialSerializer
    {
    public:
        MaterialSerializer(const Ref<Material>& material);

        bool Serialize(const Path& filepath);
        void Deserialize(const Path& filepath);

        String SerializeToString();
        bool DeserializeFromString(const String& yamlString);

    private:
        Ref<Material> m_Material;
    };

} // namespace Crowny
