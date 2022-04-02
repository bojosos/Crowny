#pragma once

namespace Crowny
{

    struct RecentProject
    {
        Path ProjectPath;
        std::time_t Timestamp;
    };

    class EditorSettingsSerializer;

    struct EditorSettings
    {
        // Windows
        bool ShowPhysicsColliders2D = false;
        bool ShowImGuiDemoWindow = false;
        bool ShowScriptDebugInfo = false;
        bool ShowAssetInfo = false;
		bool ShowEmptyMetadataAssetInfo = false;
        bool ShowEntityDebugInfo = false;

        // Snap
        glm::vec3 GridMoveSnap = glm::vec3(0.1f);
        float GridRotateSnap = 15.0f;
        float GridScaleSnap = 0.1f;

        // Project things
        Array<RecentProject, 5> RecentProjects;
        Path LastOpenProject;
        bool AutoLoadLastProject = true;

        // Console
        bool EnableConsoleInfoMessages = true;
		bool EnableConsoleWarningMessages  = true;
		bool EnableConsoleErrorMessages = true;
		bool CollapseConsole = false;
		bool ScrollToBottom = true;

        using Serializer = EditorSettingsSerializer;
    };

} // namespace Crowny