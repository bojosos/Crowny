#include "cwepch.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Serialization/ProjectSettingsSerializer.h"

#include "Crowny/Input/InputMapSerialization.h"
#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{
    namespace
    {
        constexpr size_t MAX_RECENT_SCENES = 5;

        bool NormalizeRecentScenes(Vector<UUID>& sceneIds)
        {
            Vector<UUID> normalized;
            normalized.reserve(std::min(sceneIds.size(), MAX_RECENT_SCENES));
            for (const UUID& sceneId : sceneIds)
            {
                if (sceneId.Empty() || std::find(normalized.begin(), normalized.end(), sceneId) != normalized.end())
                    continue;
                normalized.push_back(sceneId);
                if (normalized.size() == MAX_RECENT_SCENES)
                    break;
            }
            if (normalized == sceneIds)
                return false;
            sceneIds = std::move(normalized);
            return true;
        }

    } // namespace

    void ProjectSettingsSerializer::Serialize(const Ref<ProjectSettings>& settings, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny Project Settings");
        out << YAML::BeginMap;
        out << YAML::Key << "EditorCameraDistance" << YAML::Value << settings->EditorCameraDistance;
        out << YAML::Key << "EditorCameraFocalPoint" << YAML::Value << settings->EditorCameraFocalPoint;
        out << YAML::Key << "EditorCameraPosition" << YAML::Value << settings->EditorCameraPosition;
        out << YAML::Key << "EditorCameraRotation" << YAML::Value << settings->EditorCameraRotation;
        out << YAML::Key << "LastOpenSceneId" << YAML::Value << settings->LastOpenSceneId;
        out << YAML::Key << "GameStartupScene" << YAML::Value << settings->GameStartupScene;
        out << YAML::Key << "GameBuildOutput" << YAML::Value << settings->GameBuildOutput.generic_string();
        out << YAML::Key << "GameBuildDevelopment" << YAML::Value << settings->GameBuildDevelopment;
        out << YAML::Key << "RecentSceneIds" << YAML::Value << YAML::BeginSeq;
        for (const UUID& sceneId : settings->RecentSceneIds)
            out << sceneId;
        out << YAML::EndSeq;
        if (!settings->LegacyLastOpenScenePath.empty())
            out << YAML::Key << "LastOpenScene" << YAML::Value << settings->LegacyLastOpenScenePath.string();
        if (!settings->LegacyRecentScenePaths.empty())
        {
            out << YAML::Key << "RecentScenes" << YAML::Value << YAML::BeginSeq;
            for (const Path& path : settings->LegacyRecentScenePaths)
                out << path.string();
            out << YAML::EndSeq;
        }
        out << YAML::Key << "GizmoMode" << YAML::Value << (uint32_t)settings->GizmoMode; // TODO: Maybe move to project settings
        out << YAML::Key << "GizmoLocalMode" << YAML::Value << settings->GizmoLocalMode;
        out << YAML::Key << "LastAssetBrowserEntry" << YAML::Value << settings->LastAssetBrowserSelectedEntry.string();
        out << YAML::Key << "LastSelectedEntity" << YAML::Value << settings->LastSelectedEntityID;

        out << YAML::Key << "Hierarchy" << YAML::Value << YAML::BeginSeq;
        for (const UUID& uuid : settings->ExpandedEntities)
            out << uuid;
        out << YAML::EndSeq;

        SerializeInputMap(settings->InputActions, out);

        out << YAML::Key << "ManagedAssemblyReferences" << YAML::Value << YAML::BeginSeq;
        for (const Path& assembly : settings->ManagedAssemblyReferences)
            out << assembly.generic_string();
        out << YAML::EndSeq;

        out << YAML::EndMap;
    }

    Ref<ProjectSettings> ProjectSettingsSerializer::Deserialize(const YAML::Node& node)
    {
        Ref<ProjectSettings> projectSettings = CreateRef<ProjectSettings>();
        projectSettings->EditorCameraDistance = node["EditorCameraDistance"].as<float>();
        projectSettings->EditorCameraFocalPoint = node["EditorCameraFocalPoint"].as<glm::vec3>();
        projectSettings->EditorCameraPosition = node["EditorCameraPosition"].as<glm::vec3>();
        projectSettings->EditorCameraRotation = node["EditorCameraRotation"].as<glm::vec2>();
        projectSettings->GizmoMode = (GizmoEditMode)node["GizmoMode"].as<uint32_t>();
        projectSettings->LastAssetBrowserSelectedEntry = node["LastAssetBrowserEntry"].as<String>();
        projectSettings->LastOpenSceneId = node["LastOpenSceneId"].as<UUID>(UUID::EMPTY);
        projectSettings->GameStartupScene = node["GameStartupScene"].as<UUID>(UUID::EMPTY);
        projectSettings->GameBuildOutput = node["GameBuildOutput"].as<String>("");
        projectSettings->GameBuildDevelopment = node["GameBuildDevelopment"].as<bool>(false);
        projectSettings->LastSelectedEntityID = node["LastSelectedEntity"].as<UUID>(UUID::EMPTY);

        if (const YAML::Node recentSceneIds = node["RecentSceneIds"]; recentSceneIds && recentSceneIds.IsSequence())
        {
            for (const YAML::Node& sceneId : recentSceneIds)
                projectSettings->RecentSceneIds.push_back(sceneId.as<UUID>(UUID::EMPTY));
        }
        NormalizeRecentScenes(projectSettings->RecentSceneIds);

        if (const YAML::Node legacyLastScene = node["LastOpenScene"])
            projectSettings->LegacyLastOpenScenePath = legacyLastScene.as<String>(String());
        if (const YAML::Node legacyRecentScenes = node["RecentScenes"]; legacyRecentScenes && legacyRecentScenes.IsSequence())
        {
            for (const YAML::Node& path : legacyRecentScenes)
                projectSettings->LegacyRecentScenePaths.emplace_back(path.as<String>());
        }

        if (const auto& hierarchy = node["Hierarchy"])
        {
            for (const auto& uuid : hierarchy)
                projectSettings->ExpandedEntities.insert(uuid.as<UUID>());
        }

        if (const YAML::Node inputNode = node["Input"])
            projectSettings->InputActions = DeserializeInputMap(inputNode);

        if (const YAML::Node assemblies = node["ManagedAssemblyReferences"]; assemblies && assemblies.IsSequence())
        {
            Set<String> seen;
            for (const YAML::Node& assembly : assemblies)
            {
                const Path path = assembly.as<String>(String());
                if (path.empty())
                    continue;
                const String key = path.generic_string();
                if (seen.insert(key).second)
                    projectSettings->ManagedAssemblyReferences.push_back(path);
            }
        }

        return projectSettings;
    }

    bool ProjectSettingsSerializer::MigrateLegacySceneReferences(ProjectSettings& settings, const ScenePathResolver& resolver)
    {
        bool changed = false;
        changed = NormalizeRecentScenes(settings.RecentSceneIds);

        if (!settings.LastOpenSceneId.Empty())
        {
            changed = changed || !settings.LegacyLastOpenScenePath.empty();
            settings.LegacyLastOpenScenePath.clear();
        }
        else if (!settings.LegacyLastOpenScenePath.empty() && resolver)
        {
            UUID sceneId;
            if (resolver(settings.LegacyLastOpenScenePath, sceneId) && !sceneId.Empty())
            {
                settings.LastOpenSceneId = sceneId;
                settings.LegacyLastOpenScenePath.clear();
                changed = true;
            }
        }

        Vector<Path> unresolvedPaths;
        unresolvedPaths.reserve(settings.LegacyRecentScenePaths.size());
        for (const Path& path : settings.LegacyRecentScenePaths)
        {
            UUID sceneId;
            if (resolver && resolver(path, sceneId) && !sceneId.Empty())
            {
                if (std::find(settings.RecentSceneIds.begin(), settings.RecentSceneIds.end(), sceneId) == settings.RecentSceneIds.end())
                    settings.RecentSceneIds.push_back(sceneId);
                changed = true;
            }
            else if (std::find(unresolvedPaths.begin(), unresolvedPaths.end(), path) == unresolvedPaths.end())
                unresolvedPaths.push_back(path);
        }
        if (unresolvedPaths.size() != settings.LegacyRecentScenePaths.size())
            changed = true;
        settings.LegacyRecentScenePaths = std::move(unresolvedPaths);
        changed = NormalizeRecentScenes(settings.RecentSceneIds) || changed;
        return changed;
    }

    void ProjectSettingsSerializer::AddRecentScene(ProjectSettings& settings, const UUID& sceneId)
    {
        if (sceneId.Empty())
            return;
        const auto existing = std::find(settings.RecentSceneIds.begin(), settings.RecentSceneIds.end(), sceneId);
        if (existing != settings.RecentSceneIds.end())
            settings.RecentSceneIds.erase(existing);
        settings.RecentSceneIds.insert(settings.RecentSceneIds.begin(), sceneId);
        NormalizeRecentScenes(settings.RecentSceneIds);
    }

} // namespace Crowny
