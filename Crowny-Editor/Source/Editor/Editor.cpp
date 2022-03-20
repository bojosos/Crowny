#include "cwepch.h"

#include "Editor/Editor.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Serialization/ProjectSettingsSerializer.h"
#include "Serialization/EditorSettingsSerializer.h"
#include "Crowny/Serialization/FileEncoder.h"

#include "Editor/ProjectLibrary.h"

namespace Crowny
{

    void Editor::OnStartUp()
    {
		LoadEditorSettings();
        m_ProjectSettings = CreateRef<ProjectSettings>();
        ProjectLibrary::StartUp();
    }

    void Editor::CreateProject(const Path& projectParentPath, const String& projectName)
    {
        Path projectPath = projectParentPath / projectName;
        Path assetDirectory = projectPath / ProjectLibrary::ASSET_DIR;
        Path assetCache = projectPath / ProjectLibrary::INTERNAL_ASSET_DIR;

        if (!fs::exists(projectPath))
            fs::create_directories(projectPath);

        if (!fs::exists(assetDirectory))
            fs::create_directories(assetDirectory);

        if (!fs::exists(assetCache))
            fs::create_directories(assetCache);
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
                FileDecoder<ProjectSettings, SerializerType::Yaml> decoder(settingsPath);
                m_ProjectSettings = decoder.Decode();
            }
        }
        if (m_ProjectSettings == nullptr)
            m_ProjectSettings = CreateRef<ProjectSettings>();
    }

    void Editor::LoadEditorSettings()
    {
		Path settingsPath = "Editor/Settings.yaml";
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
        FileEncoder<ProjectSettings, SerializerType::Yaml> encoder(absPath);
        encoder.Encode(m_ProjectSettings);
    }

    void Editor::SaveEditorSettings()
    {
        if (m_EditorSettings == nullptr)
            return;
        Path settingsPath = "Editor/Settings.yaml";
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