#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Common/FileSystem.h"
#include "Editor/Settings/ProjectSettings.h"

#include "Editor/Settings/EditorSettings.h"

namespace Crowny
{
    enum class CodeEditorVersion
    {
        VS2008,
        VS2010,
        VS2012,
        VS2013,
        VS2015,
        VS2017,
        VS2019,
        VS2022,
        MonoDevelop,
        None
    };

    static constexpr const char* SCRIPT_EDITOR_ASSEMBLY = "CrownyEditorScript";
    static constexpr const char* EDITOR_NS = "Crowny.Editor";
    static const Path PROJECT_INTERNAL_DIR = u8"Internal";
    static const Path INTERNAL_ASSEMBLY_PATH = PROJECT_INTERNAL_DIR / "Assemblies/Debug";
    static const DialogFilter SCENE_FILTER = { "CrownyScene", "*.cwscene" };
    // Molq?
    static const Path DEFAULT_PROJECT_PATH_WIN32 = "C:/dev/Projects";
    static const Path DEFAULT_PROJECT_PATH_LINUX = "/home/life/Desktop/dev";

    class Editor : public Module<Editor>
    {
    public:
        ~Editor() = default;
        const Path& GetProjectPath() const { return m_ProjectPath; }
        const String& GetProjectName() const { return m_ProjectName; }
        bool IsProjectLoaded() const { return !m_ProjectPath.empty(); }

        void CreateProject(const Path& path, const String& projectName);
        void SaveProject();
        void LoadProject(const Path& path);
        void LoadProjectSettings();
        void SaveProjectSettings();

        void LoadEditorSettings();
        void SaveEditorSettings();

        void UnloadProject();

        Ref<ProjectSettings> GetProjectSettings() const { return m_ProjectSettings; }
        Ref<EditorSettings> GetEditorSettings() const { return m_EditorSettings; }

        static const DialogFilter& GetSceneDialogFilter() { return SCENE_FILTER; }
        const Path& GetDefaultProjectPath() const
        {
#ifdef CW_WINDOWS
            return DEFAULT_PROJECT_PATH_WIN32;
#else
            return DEFAULT_PROJECT_PATH_LINUX;
#endif
        }

    private:
        virtual void OnStartUp() override;
        virtual void OnShutdown() override;

    private:
        Path m_ProjectPath;
        Ref<ProjectSettings> m_ProjectSettings;
        Ref<EditorSettings> m_EditorSettings;
        String m_ProjectName;
    };

} // namespace Crowny