#pragma once

#include "Crowny/Common/Uuid.h"
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

        void Serialize(const std::string& filepath);
        void SerializeEntity(YAML::Emitter& out, const Uuid& uuid, Entity entity);
        void SerializeBinary(const std::string& filepath);

        void Deserialize(const std::string& filepath);
        void DeserializeBinary(const std::string& filepath);

    private:
        Ref<Scene> m_Scene;
    };
} // namespace Crowny