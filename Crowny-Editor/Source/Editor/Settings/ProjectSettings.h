#pragma once

#include "Crowny/Assets/CerealDataStreamArchive.h"

namespace Crowny
{

    struct ProjectSettings
    {
        Path LastAssetBrowserSelectedEntry;
        String LastOpenSceneName;
        int32_t GizmoMode;
        glm::vec3 EditorCameraPosition;
        glm::vec3 EditorCameraRotation;
        // TODO: slightly annoying, but implement expanded hierarchy entities serialization
    };

    void Save(BinaryDataStreamOutputArchive& archive, const ProjectSettings& settings)
    {
        archive(settings.LastAssetBrowserSelectedEntry.string());
        archive(settings.LastOpenSceneName);
        archive(settings.GizmoMode);
    }

    void Load(BinaryDataStreamOutputArchive& archive, ProjectSettings& settings)
    {
        String output;
        archive(output);
        settings.LastAssetBrowserSelectedEntry = Path(output);
        archive(settings.GizmoMode);
    }

} // namespace Crowny