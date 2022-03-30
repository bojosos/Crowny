#pragma once

#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    struct ProjectSettings;

    class ProjectSettingsSerializer
    {
    public:
        static void Serialize(const Ref<ProjectSettings>& settings, YAML::Emitter& out);
        static Ref<ProjectSettings> Deserialize(const YAML::Node& node);
    };
} // namespace Crowny