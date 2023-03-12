#pragma once

#include "Crowny/Common/Yaml.h"

namespace Crowny
{
    struct TimeSettings;

    class TimeSettingsSerializer
    {
    public:
        static void Serialize(const Ref<TimeSettings>& settings, YAML::Emitter& out);
        static Ref<TimeSettings> Deserialize(const YAML::Node& node);
    };

    struct Physics2DSettings;

    class Physics2DSettingsSerializer
    {
    public:
        static void Serialize(const Ref<Physics2DSettings>& settings, YAML::Emitter& out);
        static Ref<Physics2DSettings> Deserialize(const YAML::Node& node);
    };
} // namespace Crowny