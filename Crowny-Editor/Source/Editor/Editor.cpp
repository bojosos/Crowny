#include "cwepch.h"

#include "Editor/Editor.h"

#include "Crowny/Assets/CerealDataStreamArchive.h"
#include "Editor/ProjectLibrary.h"

namespace Crowny
{

    void Editor::CreateProject(const Path& projectParentPath, const String& projectName)
    {
        Path projectPath = projectParentPath / projectName;
        Path assetDirectory = projectPath / ProjectLibrary::ASSET_DIR;
        Path assetCache = projectPath / ProjectLibrary::INTERNAL_ASSET_DIR;

        if (!fs::exists(projectPath))
            fs::create_directory(projectPath);

        if (!fs::exists(assetDirectory))
            fs::create_directory(assetDirectory);

        if (!fs::exists(assetCache))
            fs::create_directory(assetCache);
    }

    void Editor::LoadProject(const Path& projectPath)
    {
        UnloadProject();
        m_ProjectPath = projectPath;
        m_ProjectName = projectPath.filename().string();

        LoadProjectSettings();
        ProjectLibrary::Get().LoadLibrary();
    }

    void Editor::LoadProjectSettings()
    {
        if (IsProjectLoaded())
        {
            Path settingsPath = m_ProjectPath / "ProjectSettings.yaml";
            if (fs::exists(settingsPath))
            {
                Ref<DataStream> stream = FileSystem::OpenFile(settingsPath);
                BinaryDataStreamInputArchive archive(stream);
                archive(m_ProjectSettings);
                stream->Close();
            }
        }
        if (m_ProjectSettings == nullptr)
            m_ProjectSettings = CreateRef<ProjectSettings>();
    }

    void Editor::UnloadProject()
    {
        if (!IsProjectLoaded())
            return;
        SaveProject();
        ProjectLibrary::Get().UnloadLibrary();
        m_ProjectPath = "";
        m_ProjectName = "";
    }

    void Editor::SaveProjectSettings()
    {
        if (m_ProjectSettings == nullptr || !IsProjectLoaded())
            return;

        Path absPath = GetProjectPath() / "ProjectSettings.yaml";

        if (!fs::is_directory(absPath.parent_path()))
            fs::create_directories(absPath.parent_path());
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(absPath);
        BinaryDataStreamOutputArchive archive(stream);
        archive(m_ProjectSettings);
        stream->Close();
    }

    void Editor::SaveProject()
    {
        if (!IsProjectLoaded())
            return;
        // SaveEditorSettings();
        SaveProjectSettings();
        ProjectLibrary::Get().SaveLibrary();
    }

} // namespace Crowny