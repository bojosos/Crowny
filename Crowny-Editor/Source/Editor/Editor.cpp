#include "cwepch.h"

#include "Editor/Editor.h"

#include "Crowny/Assets/CerealDataStreamArchive.h"
#include "Editor/ProjectLibrary.h"

namespace Crowny
{

    void Editor::CreateProject(const Path& projectParentPath, const String& projectName)
    {
        Path projectPath = projectParentPath / projectName;
        Path assetDirectory = projectPath / "Assets";
        Path assetCache = projectPath / "Cache";

        if (!std::filesystem::exists(projectPath))
            std::filesystem::create_directory(projectPath);

        if (!std::filesystem::exists(assetDirectory))
            std::filesystem::create_directory(assetDirectory);

        if (!std::filesystem::exists(assetCache))
            std::filesystem::create_directory(assetCache);
    }

    void Editor::LoadProject(const Path& projectPath)
    {
        UnloadProject();
        m_ProjectPath = projectPath;
        m_ProjectName = projectPath.filename();

        LoadProjectSettings();
        ProjectLibrary::Get().LoadLibrary();
    }

    void Editor::LoadProjectSettings()
    {
        if (IsProjectLoaded())
        {
            Path settingsPath = m_ProjectPath / "ProjectSettings.yaml";
            if (std::filesystem::exists(settingsPath))
            {
                Ref<DataStream> stream = FileSystem::OpenFile(settingsPath);
                BinaryDataStreamInputArchive archive(stream);
                archive(m_ProjectSettings);
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

        Path absPath = GetProjectPath();
        absPath.append("ProjectSettings.yaml");

        Ref<DataStream> stream = FileSystem::OpenFile(absPath, false);
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