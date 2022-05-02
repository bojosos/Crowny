#include "cwepch.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Serialization/ProjectSettingsSerializer.h"

namespace Crowny
{

    void ProjectSettingsSerializer::Serialize(const Ref<ProjectSettings>& settings, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny Project Settings");
        out << YAML::BeginMap;
        out << YAML::Key << "EditorCameraDistance" << YAML::Value << settings->EditorCameraDistance;
        out << YAML::Key << "EditorCameraFocalPoint" << YAML::Value << settings->EditorCameraFocalPoint;
        out << YAML::Key << "EditorCameraPosition" << YAML::Value << settings->EditorCameraPosition;
        out << YAML::Key << "EditorCameraRotation" << YAML::Value << settings->EditorCameraRotation;
        out << YAML::Key << "LastOpenScene" << YAML::Value << settings->LastOpenScenePath;
        out << YAML::Key << "GizmoMode" << YAML::Value << (uint32_t)settings->GizmoMode; // TODO: Maybe move to project settings
        out << YAML::Key << "GizmoLocalMode" << YAML::Value << settings->GizmoLocalMode;
        out << YAML::Key << "LastAssetBrowserEntry" << YAML::Value << settings->LastAssetBrowserSelectedEntry.string();
        out << YAML::Key << "LastSelectedEntity" << YAML::Value << settings->LastSelectedEntityID;

        out << YAML::Key << "LayerNames" << YAML::Value << YAML::BeginSeq;
        for (uint32_t i = 0; i < settings->LayerNames.size(); i++)
            out << settings->LayerNames[i];
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
        projectSettings->LastOpenScenePath = node["LastOpenScene"].as<String>();

        if (const auto& id = node["LastSelectedEntity"])
            projectSettings->LastSelectedEntityID = id.as<UUID>();
        if (const auto& layerNames = node["LayerNames"])
        {
            uint32_t idx = 0;
            for (const auto& layerName : layerNames)
                projectSettings->LayerNames[idx++] = layerName.as<String>();
        }
        return projectSettings;
    }

} // namespace Crowny