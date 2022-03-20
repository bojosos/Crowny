#pragma once

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Serialization/SerializeUtils.h"

namespace Crowny
{

    class ProjectSettingsSerializer;

    struct ProjectSettings
    {
        Path LastAssetBrowserSelectedEntry;
        String LastOpenScenePath; // This will probably be replaced by the UUID of a scene, and a scene will most likely be a prefab.
        int32_t GizmoMode;
        glm::vec3 EditorCameraPosition;
        glm::vec3 EditorCameraFocalPoint;
        glm::vec2 EditorCameraRotation;
        float EditorCameraDistance;

        using Serializer = ProjectSettingsSerializer;

        // TODO: slightly annoying, but implement expanded hierarchy entities serialization
        // vector with uuids of expanded entities? Need to use the "open" from HierarchyPanel:112 and somehow build a
        // list from that
    };

} // namespace Crowny