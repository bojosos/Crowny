#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    struct BuildSceneOption
    {
        UUID Id;
        Path SourcePath;
        String DisplayName;
    };

    inline Vector<BuildSceneOption> MakeBuildSceneOptions(Vector<BuildSceneOption> scenes, const Path& assetFolder)
    {
        Set<UUID> seen;
        std::erase_if(scenes,
                      [&](const BuildSceneOption& scene) { return scene.Id.Empty() || scene.SourcePath.empty() || !seen.insert(scene.Id).second; });
        for (auto& scene : scenes)
        {
            const Path relative = scene.SourcePath.lexically_normal().lexically_relative(assetFolder.lexically_normal());
            scene.DisplayName = (!relative.empty() && *relative.begin() != ".." ? relative : scene.SourcePath).generic_string();
        }
        std::sort(scenes.begin(), scenes.end(), [](const auto& left, const auto& right) { return left.DisplayName < right.DisplayName; });
        return scenes;
    }
} // namespace Crowny
