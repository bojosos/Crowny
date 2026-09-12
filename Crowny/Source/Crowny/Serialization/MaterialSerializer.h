#pragma once

#include "Crowny/Renderer/Material.h"

namespace YAML
{
    class Node;
}

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

        // Assigned texture assets only; generated defaults and shader bindings are not content roots.
        static Vector<UUID> GatherTextureDependencies(const YAML::Node& source);

    private:
        Ref<Material> m_Material;
    };

} // namespace Crowny
