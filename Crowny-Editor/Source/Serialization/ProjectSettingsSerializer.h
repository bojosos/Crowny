#pragma once

#include "Crowny/Common/Yaml.h"

#include <functional>

namespace Crowny
{

    struct ProjectSettings;

    class ProjectSettingsSerializer
    {
    public:
        using ScenePathResolver = std::function<bool(const Path&, UUID&)>;

        static void Serialize(const Ref<ProjectSettings>& settings, YAML::Emitter& out);
        static Ref<ProjectSettings> Deserialize(const YAML::Node& node);

        static bool MigrateLegacySceneReferences(ProjectSettings& settings, const ScenePathResolver& resolver);
        static void AddRecentScene(ProjectSettings& settings, const UUID& sceneId);
    };

} // namespace Crowny
