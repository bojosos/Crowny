#include "cwepch.h"

#include "Editor/Editor.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Editor/Settings/EditorSettingsPersistence.h"
#include "Serialization/EditorSettingsSerializer.h"
#include "Serialization/ProjectSettingsSerializer.h"

#include "Editor/ProjectLibrary.h"

namespace Crowny
{
    namespace
    {
        Path GetPersistentEditorSettingsPath() { return PlatformUtils::GetOurRoamingDirectory() / "Editor/Settings.yaml"; }

        Path GetLegacyEditorSettingsPath()
        {
            std::error_code error;
            const Path processRelativePath = fs::absolute("Editor/Settings.yaml", error);
            const Path normalizedProcessPath = error ? Path("Editor/Settings.yaml") : processRelativePath.lexically_normal();
            if (fs::is_regular_file(normalizedProcessPath))
                return normalizedProcessPath;

            if (Application::TryGet() != nullptr)
            {
                const Path workingDirectory = Application::TryGet()->GetWorkingDirectory();
                const Path repositoryLegacyPath = workingDirectory / "Crowny-Editor/Editor/Settings.yaml";
                if (fs::is_regular_file(repositoryLegacyPath))
                    return repositoryLegacyPath;

                const Path workingDirectoryLegacyPath = workingDirectory / "Editor/Settings.yaml";
                if (fs::is_regular_file(workingDirectoryLegacyPath))
                    return workingDirectoryLegacyPath;
            }
            return normalizedProcessPath;
        }

        Path CanonicalizeProjectPath(const Path& projectPath)
        {
            std::error_code error;
            const Path canonicalPath = fs::weakly_canonical(projectPath, error);
            if (!error && !canonicalPath.empty())
                return NormalizeProjectPath(canonicalPath);

            error.clear();
            const Path absolutePath = fs::absolute(projectPath, error);
            return NormalizeProjectPath(error ? projectPath : absolutePath);
        }
    } // namespace

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

        const Path canonicalProjectPath = CanonicalizeProjectPath(projectPath);

        const Path assetsPath = canonicalProjectPath / "Assets";
        if (fs::exists(assetsPath))
            m_Watch = CreateScope<FileWatch>(assetsPath, m_FileWatchCallback);

        m_ProjectPath = canonicalProjectPath;
        m_ProjectName = canonicalProjectPath.filename().string();

        LoadProjectSettings();
        ProjectLibrary::Get().LoadLibrary();
        const bool migratedSceneReferences = ProjectSettingsSerializer::MigrateLegacySceneReferences(
          *m_ProjectSettings, [](const Path& path, UUID& sceneId) { return ProjectLibrary::Get().TryGetAssetId(path, AssetType::Scene, sceneId); });
        if (migratedSceneReferences)
            SaveProjectSettings();
        RecordRecentProject(*m_EditorSettings, canonicalProjectPath, std::time(nullptr));
        SaveEditorSettings();
        Log::RenameClientLogger(canonicalProjectPath.filename().string());
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
        const Path persistentPath = GetPersistentEditorSettingsPath();
        const Path legacyPath = GetLegacyEditorSettingsPath();
        const EditorSettingsPaths paths =
          SelectEditorSettingsPaths(persistentPath, legacyPath, fs::is_regular_file(persistentPath), fs::is_regular_file(legacyPath));
        if (!paths.LoadPath.empty())
        {
            FileDecoder<EditorSettings, SerializerType::Yaml> decoder(paths.LoadPath);
            m_EditorSettings = decoder.Decode();
        }
        if (m_EditorSettings == nullptr)
            m_EditorSettings = CreateRef<EditorSettings>();
        const bool normalizedProjects =
          NormalizeRecentProjects(*m_EditorSettings, [](const Path& path) { return CanonicalizeProjectPath(path); });
        if (paths.MigrateLegacy || normalizedProjects)
            SaveEditorSettings();
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
        const Path settingsPath = GetPersistentEditorSettingsPath();
        if (!fs::is_directory(settingsPath.parent_path()))
            fs::create_directories(settingsPath.parent_path());
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
