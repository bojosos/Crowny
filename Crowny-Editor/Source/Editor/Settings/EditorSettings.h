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
        bool ShowPhysicsColliders2D = false;
        bool ShowImGuiDemoWindow = false;

        glm::vec3 GridMoveSnap = glm::vec3(0.1f);

        float GridRotateSnap = 15.0f;

        float GridScaleSnap = 0.1f;

        Array<RecentProject, 5> RecentProjects;
        Path LastOpenProject;
        bool AutoLoadLastProject = true;

        bool EnableConsoleInfoMessages  = true;
		bool EnableConsoleWarningMessages  = true;
		bool EnableConsoleErrorMessages  = true;
		bool EnableConsoleCriticalMessages = true;
		
		bool CollapseConsole = false;

        using Serializer = EditorSettingsSerializer;
    };

} // namespace Crowny