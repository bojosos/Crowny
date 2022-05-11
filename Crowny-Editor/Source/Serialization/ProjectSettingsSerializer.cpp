#include "cwepch.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Serialization/ProjectSettingsSerializer.h"

#include "Crowny/Physics/Physics2D.h"

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
        out << YAML::Key << "GizmoMode" << YAML::Value
            << (uint32_t)settings->GizmoMode; // TODO: Maybe move to project settings
        out << YAML::Key << "GizmoLocalMode" << YAML::Value << settings->GizmoLocalMode;
        out << YAML::Key << "LastAssetBrowserEntry" << YAML::Value << settings->LastAssetBrowserSelectedEntry.string();
        out << YAML::Key << "LastSelectedEntity" << YAML::Value << settings->LastSelectedEntityID;

        out << YAML::Key << "LayerNames" << YAML::Value << YAML::BeginSeq;
        for (uint32_t i = 0; i < 32; i++)
            out << Physics2D::Get().GetLayerName(i);
        out << YAML::EndSeq;
        out << YAML::Key << "Gravity2D" << YAML::Value << Physics2D::Get().GetGravity();
        // out << YAML::Key << "DefaultMaterial" << YAML::Value << Physics2D::Get().GetDefaultMaterial();
        out << YAML::Key << "VelocityIterations" << YAML::Value << Physics2D::Get().GetVelocityIterations();
        out << YAML::Key << "PositionIterations" << YAML::Value << Physics2D::Get().GetPositionIterations();

        out << YAML::Key << "CollisionMatrix" << YAML::Value;
        out << YAML::BeginSeq;
        for (uint32_t i = 0; i < 32; i++)
            out << Physics2D::Get().GetCategoryMask(i);
        out << YAML::EndSeq;

        out << YAML::Key << "Hierarchy" << YAML::Value << YAML::BeginSeq;
        for (const UUID& uuid : settings->ExpandedEntities)
            out << uuid;
        out << YAML::EndSeq;

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
        projectSettings->LastSelectedEntityID = node["LastSelectedEntity"].as<UUID>(UUID::EMPTY);
        if (const auto& layerNames = node["LayerNames"])
        {
            uint32_t idx = 0;
            for (const auto& layerName : layerNames)
                Physics2D::Get().SetLayerName(idx++, layerName.as<String>());
        }
        Physics2D::Get().SetGravity(node["Gravity2D"].as<glm::vec2>(glm::vec2(0.0f, -9.81f)));
        Physics2D::Get().SetVelocityIterations(node["VelocityIterations"].as<uint32_t>(8));
        Physics2D::Get().SetPositionIterations(node["PositionIterations"].as<uint32_t>(3));

        const auto& collisionMatrix = node["CollisionMatrix"];
        if (collisionMatrix)
        {
            uint32_t i = 0;
            for (const auto& entry : collisionMatrix)
                Physics2D::Get().SetCategoryMask(i++, entry.as<uint32_t>());
        }

        if (const auto& hierarchy = node["Hierarchy"])
        {
            for (const auto& uuid : hierarchy)
                projectSettings->ExpandedEntities.insert(uuid.as<UUID>());
        }
        return projectSettings;
    }

} // namespace Crowny