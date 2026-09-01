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
        UUID LastOpenSceneId;
        Vector<UUID> RecentSceneIds;

        // Kept only until legacy path-based settings can be resolved through the project asset index.
        Path LegacyLastOpenScenePath;
        Vector<Path> LegacyRecentScenePaths;
        UUID LastSelectedEntityID;
        UnorderedSet<UUID> ExpandedEntities;

        // Cameras
        GizmoEditMode GizmoMode = GizmoEditMode::Translate;
        bool GizmoLocalMode = false;

        // Project-authored input contexts and bindings. Runtime rebinds remain in memory.
        InputMap InputActions;

        // Pure managed .dll files referenced by game scripts. Paths are relative to the project when possible.
        Vector<Path> ManagedAssemblyReferences;

        // Camera
        glm::vec3 EditorCameraPosition = { 0.0f, 0.0f, 0.0f };
        glm::vec3 EditorCameraFocalPoint = { 0.0f, 0.0f, 0.0f };
        glm::vec2 EditorCameraRotation = { 0.0f, 0.0f };
        float EditorCameraDistance = 10.0f;

        using Serializer = ProjectSettingsSerializer;
    };

} // namespace Crowny
