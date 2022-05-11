#pragma once

#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    class PhysicsMaterial2D;

    class PhysicsMaterial2DSerializer
    {
    public:
        static void Serialize(const Ref<PhysicsMaterial2D>& manifest, YAML::Emitter& out);
        static Ref<PhysicsMaterial2D> Deserialize(const YAML::Node& node);
    };
} // namespace Crowny