#pragma once

#include "Crowny/Common/Module.h"

#include "Editor/Settings/ProjectSettings.h"

namespace Crowny
{

    static constexpr const char* SCRIPT_EDITOR_ASSEMBLY = "CrownyEditorScript";
    static constexpr const char* EDITOR_NS = "Crowny.Editor";
    static const Path PROJECT_INTERNAL_DIR = u8"Internal";
    static const Path INTERNAL_ASSEMBLY_PATH = PROJECT_INTERNAL_DIR / "Assemblies";

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