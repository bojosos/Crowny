#pragma once

#include "Crowny/Assets/CerealDataStreamArchive.h"

#include "Crowny/Assets/SerializeUtils.h"

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
        // vector with uuids of expanded entities? Need to use the "open" from HierarchyPanel:112 and somehow build a
        // list from that
    };

    template <class Archive> void Save(Archive& archive, const ProjectSettings& settings)
    {
        String str = settings.LastAssetBrowserSelectedEntry.string();
        archive(str);
        archive(settings.LastOpenSceneName);
        archive(settings.GizmoMode);
        archive(settings.EditorCameraPosition);
        archive(settings.EditorCameraRotation);
    }

    template <class Archive> void Load(Archive& archive, ProjectSettings& settings)
    {
        String output;
        archive(output);
        settings.LastAssetBrowserSelectedEntry = Path(output);
        archive(settings.LastOpenSceneName);
        archive(settings.GizmoMode);
        archive(settings.EditorCameraPosition);
        archive(settings.EditorCameraRotation);
    }

} // namespace Crowny