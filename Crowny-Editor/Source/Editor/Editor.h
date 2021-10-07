#pragma once

#include "Crowny/Common/Module.h"

#include "Editor/Settings/ProjectSettings.h"

namespace Crowny
{

    class Editor : public Module<Editor>
    {
    public:
        const Path& GetProjectPath() const { return m_ProjectPath; }
        bool IsProjectLoaded() const { return !m_ProjectPath.empty(); }

        void CreateProject(const Path& path, const String& projectName);
        void SaveProject();
        void LoadProject(const Path& path);
        void LoadProjectSettings();
        void SaveProjectSettings();

        void UnloadProject();

    private:
        Path m_ProjectPath;
        Ref<ProjectSettings> m_ProjectSettings;
        String m_ProjectName;
    };

} // namespace Crowny