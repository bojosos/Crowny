#pragma once

namespace Crowny
{

    struct RecentProject
    {
        Path ProjectPath;
        std::time_t Timestamp;
    };

    class EditorSettingsSerializer;

    struct EditorSettings : public RefCounted
    {
        // Windows
        bool ShowPhysicsColliders = false;
        bool ShowImGuiDemoWindow = false;
        bool ShowScriptDebugInfo = false;
        bool ShowAssetInfo = false;
        bool ShowEmptyMetadataAssetInfo = false;
        bool ShowEntityDebugInfo = false;

        // Snap
        glm::vec3 GridMoveSnap = glm::vec3(0.1f);
        float GridRotateSnap = 15.0f;
        float GridScaleSnap = 0.1f;

        // Viewport
        bool WireframeMode = false;
        bool ShowRenderingStatistics = true;
        bool ShowGrid = true;
        bool ShowGridAxes = true;
        float GridFineSize = 1.0f;
        float GridCoarseSize = 10.0f;
        float GridLineWidth = 0.02f;
        float GridOpacity = 0.4f;
        glm::vec4 ColliderColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

        // Project things
        Array<RecentProject, 5> RecentProjects;
        Path LastOpenProject;
        bool AutoLoadLastProject = true;

        // Console
        bool EnableConsoleInfoMessages = true;
        bool EnableConsoleWarningMessages = true;
        bool EnableConsoleErrorMessages = true;
        bool CollapseConsole = false;
        bool ScrollToBottom = true;
        uint32_t MaxCallstackLength = 10;

        // Script inspector dictionary presentation overrides, keyed by stable ImGui property ID.
        Map<String, uint32_t> DictionaryLayouts;
        Map<String, uint32_t> DictionaryLayoutRevisions;

        // Code Editor
        Path CodeEditorPath;

        using Serializer = EditorSettingsSerializer;
    };

} // namespace Crowny
