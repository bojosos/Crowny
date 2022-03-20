#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace YAML
{
    class Emitter;
}

namespace Crowny
{

    class SceneSerializer
    {
    public:
        SceneSerializer(const Ref<Scene>& scene);

        void Serialize(const Path& filepath);
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        void SerializeBinary(const Path& filepath);

        void Deserialize(const Path& filepath);
        void DeserializeBinary(const Path& filepath);

    private:
        Ref<Scene> m_Scene;
    };
} // namespace Crowny