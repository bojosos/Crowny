#include "cwepch.h"

#include "Editor/Editor.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Serialization/EditorSettingsSerializer.h"
#include "Serialization/ProjectSettingsSerializer.h"

#include "Editor/ProjectLibrary.h"

namespace Crowny
{
    Editor::Editor(FileWatch::FileWatchCallback&& fileWatchCallback) : m_FileWatchCallback(fileWatchCallback) {}

    void Editor::OnStartUp()
    {
        LoadEditorSettings();
        m_ProjectSettings = CreateRef<ProjectSettings>();
        ProjectLibrary::StartUp();
    }

    void Editor::CreateProject(const Path& projectParentPath, const String& projectName)
    {
        const Path& projectPath = projectParentPath / projectName;
        const Path& assetDirectory = projectPath / ProjectLibrary::ASSET_DIR;
        const Path& assetCache = projectPath / ProjectLibrary::INTERNAL_ASSET_DIR;
        const Path& assembliesDir = projectPath / INTERNAL_ASSEMBLY_PATH;

        if (!fs::exists(projectPath))
            fs::create_directories(projectPath);

        if (!fs::exists(assetDirectory))
            fs::create_directories(assetDirectory);

        if (!fs::exists(assetCache))
            fs::create_directories(assetCache);

        if (!fs::exists(assembliesDir))
            fs::create_directories(assembliesDir);

        // Create a default empty scene
        const Path defaultScenePath = assetDirectory / "DefaultScene.cwscene";
        if (!fs::exists(defaultScenePath))
        {
            Ref<Scene> defaultScene = CreateRef<Scene>("DefaultScene");
            SceneSerializer serializer(defaultScene);
            serializer.Serialize(defaultScenePath);
        }
    }

    void Editor::LoadProject(const Path& projectPath)
    {
        UnloadProject();

        const Path assetsPath = projectPath / "Assets";
        if (fs::exists(assetsPath))
            m_Watch = CreateScope<FileWatch>(assetsPath, m_FileWatchCallback);

        m_ProjectPath = projectPath;
        m_ProjectName = projectPath.filename().string();

        LoadProjectSettings();
        ProjectLibrary::Get().LoadLibrary();
        Log::RenameClientLogger(projectPath.filename().string());
    }

    void Editor::LoadProjectSettings()
    {
        if (IsProjectLoaded())
        {
            const Path settingsPath = m_ProjectPath / "ProjectSettings.yaml";
            if (fs::exists(settingsPath))
            {
                FileDecoder<ProjectSettings, SerializerType::Yaml> decoder(settingsPath);
                m_ProjectSettings = decoder.Decode();
            }
        }
        if (m_ProjectSettings == nullptr)
            m_ProjectSettings = CreateRef<ProjectSettings>();
        Input::SetActionMap(m_ProjectSettings->InputActions);
    }

    void Editor::LoadEditorSettings()
    {
        const Path settingsPath = "Editor/Settings.yaml";
        if (fs::exists(settingsPath))
        {
            FileDecoder<EditorSettings, SerializerType::Yaml> decoder(settingsPath);
            m_EditorSettings = decoder.Decode();
        }
        if (m_EditorSettings == nullptr)
            m_EditorSettings = CreateRef<EditorSettings>();
    }

    void Editor::UnloadProject()
    {
        m_Watch.reset();

        if (!IsProjectLoaded())
            return;
        SaveProject();
        ProjectLibrary::Get().UnloadLibrary();
        m_ProjectPath = "";
        m_ProjectName = "";
        m_ProjectSettings = CreateRef<ProjectSettings>();
        Input::ClearActionMap();
    }

    void Editor::SaveProjectSettings()
    {
        if (m_ProjectSettings == nullptr || !IsProjectLoaded())
            return;

        const Path absPath = GetProjectPath() / "ProjectSettings.yaml";

        if (!fs::is_directory(absPath.parent_path()))
            fs::create_directories(absPath.parent_path());
        FileEncoder<ProjectSettings, SerializerType::Yaml> encoder(absPath);
        encoder.Encode(m_ProjectSettings);
    }

    void Editor::SaveEditorSettings()
    {
        if (m_EditorSettings == nullptr)
            return;
        const Path settingsPath = "Editor/Settings.yaml";
        FileEncoder<EditorSettings, SerializerType::Yaml> encoder(settingsPath);
        encoder.Encode(m_EditorSettings);
    }

    void Editor::SaveProject()
    {
        if (!IsProjectLoaded())
            return;
        SaveEditorSettings();
        SaveProjectSettings();
        ProjectLibrary::Get().SaveLibrary();
    }

    void Editor::OnShutdown()
    {
        UnloadProject();
        SaveEditorSettings();
    }

} // namespace Crowny
