#pragma once

#include "Crowny/Common/Uuid.h"

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

namespace YAML
{
    template <> struct convert<Crowny::Uuid>
    {
        static Node encode(const Crowny::Uuid& uuid)
        {
            Node node;
            node = uuid.ToString();
            return node;
        }

        static bool decode(const Node& node, Crowny::Uuid& rhs)
        {
            if (!node.IsScalar())
                return false;
            rhs = Crowny::Uuid(node.as<std::string>());

            return true;
        }
    };

    template <> struct convert<glm::vec3>
    {
        static Node encode(const glm::vec3& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();

            return true;
        }
    };

    template <> struct convert<glm::vec4>
    {
        static Node encode(const glm::vec4& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);

            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();

            return true;
        }
    };
} // namespace YAML
