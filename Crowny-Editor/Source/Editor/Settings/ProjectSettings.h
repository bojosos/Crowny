#pragma once

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Serialization/SerializeUtils.h"

#include "Panels/ViewportPanel.h" // for Gizmo mode

namespace Crowny
{

    class ProjectSettingsSerializer;

    struct ProjectSettings
    {
        Path LastAssetBrowserSelectedEntry;
        String LastOpenScenePath; // This will probably be replaced by the UUID of a scene, and a scene will most likely
                                  // be a prefab.
        GizmoEditMode GizmoMode = GizmoEditMode::Translate;
        bool GizmoLocalMode = false;

        glm::vec3 EditorCameraPosition = { 0.0f, 0.0f, 0.0f };
        glm::vec3 EditorCameraFocalPoint = { 0.0f, 0.0f, 0.0f };
        glm::vec2 EditorCameraRotation = { 0.0f, 0.0f };
        float EditorCameraDistance = 10.0f;

        UUID LastSelectedEntityID;

        using Serializer = ProjectSettingsSerializer;

        // TODO: slightly annoying, but implement expanded hierarchy entities serialization
        // vector with uuids of expanded entities? Need to use the "open" from HierarchyPanel:112 and somehow build a
        // list from that
    };

} // namespace Crowny