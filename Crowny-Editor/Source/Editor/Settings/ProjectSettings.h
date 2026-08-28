#pragma once

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Input/InputMap.h"
#include "Crowny/Serialization/SerializeUtils.h"

#include "Panels/ViewportPanel.h" // for Gizmo mode

namespace Crowny
{

    class ProjectSettingsSerializer;

    struct ProjectSettings : public RefCounted
    {
        // Asset browser
        Path LastAssetBrowserSelectedEntry;

        // Scene
        String LastOpenScenePath; // This will probably be replaced by the UUID of a scene, and a scene will most likely
                                  // be a prefab.
        Vector<Path> RecentScenes;
        UUID LastSelectedEntityID;
        UnorderedSet<UUID> ExpandedEntities;

        // Cameras
        GizmoEditMode GizmoMode = GizmoEditMode::Translate;
        bool GizmoLocalMode = false;

        // Project-authored input contexts and bindings. Runtime rebinds remain in memory.
        InputMap InputActions;

        // Camera
        glm::vec3 EditorCameraPosition = { 0.0f, 0.0f, 0.0f };
        glm::vec3 EditorCameraFocalPoint = { 0.0f, 0.0f, 0.0f };
        glm::vec2 EditorCameraRotation = { 0.0f, 0.0f };
        float EditorCameraDistance = 10.0f;

        using Serializer = ProjectSettingsSerializer;
    };

} // namespace Crowny
