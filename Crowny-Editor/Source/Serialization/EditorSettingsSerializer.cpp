#include "cwepch.h"

#include "Serialization/EditorSettingsSerializer.h"
#include "Editor/Settings/EditorSettings.h"

namespace Crowny
{
	void EditorSettingsSerializer::Serialize(const Ref<EditorSettings>& settings, YAML::Emitter& out)
	{
		out << YAML::Comment("Crowny Editor Settings");
		out << YAML::BeginMap;
		out << YAML::Key << "GridMoveSnap" << YAML::Value << settings->GridMoveSnap;
		out << YAML::Key << "GridRotateSnap" << YAML::Value << settings->GridRotateSnap;
		out << YAML::Key << "GridScaleSnap" << YAML::Value << settings->GridScaleSnap;
		out << YAML::Key << "LastOpenProject" << YAML::Value << settings->LastOpenProject.string();
		out << YAML::Key << "AutoLoadLastProject" << YAML::Value << settings->AutoLoadLastProject;
		out << YAML::Key << "ShowImGuiDemo" << YAML::Value << settings->ShowImGuiDemoWindow; // TODO: Maybe move to project settings
		out << YAML::Key << "ShowPhysicsColliders2D" << YAML::Value << settings->ShowPhysicsColliders2D;
		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
		for (RecentProject& proj : settings->RecentProjects)
		{
			if (proj.ProjectPath.empty())
				continue;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << proj.ProjectPath.string();
			out << YAML::Key << "Timestamp" << YAML::Value << proj.Timestamp;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}

	Ref<EditorSettings> EditorSettingsSerializer::Deserialize(const YAML::Node& node)
	{
		Ref<EditorSettings> editorSettings = CreateRef<EditorSettings>();
		editorSettings->GridMoveSnap = node["GridMoveSnap"].as<glm::vec3>();
		editorSettings->GridRotateSnap = node["GridRotateSnap"].as<float>();
		editorSettings->GridScaleSnap = node["GridScaleSnap"].as<float>();
		editorSettings->LastOpenProject = node["LastOpenProject"].as<String>();
		editorSettings->AutoLoadLastProject = node["AutoLoadLastProject"].as<bool>();
		editorSettings->ShowImGuiDemoWindow = node["ShowImGuiDemo"].as<bool>();
		editorSettings->ShowPhysicsColliders2D = node["ShowPhysicsColliders2D"].as<bool>();
		
		uint32_t idx = 0;
		for (auto project : node["RecentProjects"])
		{
			editorSettings->RecentProjects[idx].ProjectPath = project["Path"].as<String>();
			editorSettings->RecentProjects[idx++].Timestamp = project["Timestamp"].as<std::time_t>();
		}
		return editorSettings;
	}
}