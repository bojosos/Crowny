#pragma once

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    class SceneSerializer
    {
    public:
        SceneSerializer(const Ref<Scene>& scene);
        
        void Serialize(const std::string& filepath);
        void SerializeBinary(const std::string& filepath);

        void Deserialize(const std::string& filepath);
        void DeserializeBinary(const std::string& filepath);

    private:
        Ref<Scene> m_Scene;
    };
}