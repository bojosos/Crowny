#include "cwepch.h"

#include "Editor/Settings/EditorSettings.h"
#include "Serialization/EditorSettingsSerializer.h"

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
        out << YAML::Key << "ShowPhysicsColliders" << YAML::Value << settings->ShowPhysicsColliders;
        out << YAML::Key << "WireframeMode" << YAML::Value << settings->WireframeMode;
        out << YAML::Key << "ShowRenderingStatistics" << YAML::Value << settings->ShowRenderingStatistics;
        out << YAML::Key << "ShowGrid" << YAML::Value << settings->ShowGrid;
        out << YAML::Key << "ShowGridAxes" << YAML::Value << settings->ShowGridAxes;
        out << YAML::Key << "GridFineSize" << YAML::Value << settings->GridFineSize;
        out << YAML::Key << "GridCoarseSize" << YAML::Value << settings->GridCoarseSize;
        out << YAML::Key << "GridLineWidth" << YAML::Value << settings->GridLineWidth;
        out << YAML::Key << "GridOpacity" << YAML::Value << settings->GridOpacity;
        out << YAML::Key << "ColliderColor" << YAML::Value << settings->ColliderColor;
        out << YAML::Key << "ShowScriptDebugInfo" << YAML::Value << settings->ShowScriptDebugInfo;
        out << YAML::Key << "ShowAssetInfo" << YAML::Value << settings->ShowAssetInfo;
        out << YAML::Key << "ShowEmptyMetadataAssetInfo" << YAML::Value << settings->ShowEmptyMetadataAssetInfo;
        out << YAML::Key << "ShowEntityDebugInfo" << YAML::Value << settings->ShowEntityDebugInfo;
        out << YAML::Key << "EnableConsoleInfoMessages" << YAML::Value << settings->EnableConsoleInfoMessages;
        out << YAML::Key << "EnableConsoleWarningMessages" << YAML::Value << settings->EnableConsoleWarningMessages;
        out << YAML::Key << "EnableConsoleErrorMessages" << YAML::Value << settings->EnableConsoleErrorMessages;
        out << YAML::Key << "CollapseConsole" << YAML::Value << settings->CollapseConsole;
        out << YAML::Key << "ScrollToBottom" << YAML::Value << settings->ScrollToBottom;
        out << YAML::Key << "CodeEditorPath" << YAML::Value << settings->CodeEditorPath.string();

        out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
        for (const RecentProject& proj : settings->RecentProjects)
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

        if (const YAML::Node& info = node["ShowScriptDebugInfo"])
            editorSettings->ShowScriptDebugInfo = info.as<bool>();
        if (const YAML::Node& info = node["ShowAssetInfo"])
            editorSettings->ShowAssetInfo = info.as<bool>();
        if (const YAML::Node& info = node["ShowEmptyMetadataAssetInfo"])
            editorSettings->ShowEmptyMetadataAssetInfo = info.as<bool>();
        if (const YAML::Node& info = node["ShowEntityDebugInfo"])
            editorSettings->ShowEntityDebugInfo = info.as<bool>();

        if (const YAML::Node& infos = node["EnableConsoleInfoMessages"])
            editorSettings->EnableConsoleInfoMessages = infos.as<bool>(true);
        if (const YAML::Node& infos = node["EnableConsoleWarningMessages"])
            editorSettings->EnableConsoleWarningMessages = infos.as<bool>(true);
        if (const YAML::Node& infos = node["EnableConsoleErrorMessages"])
            editorSettings->EnableConsoleErrorMessages = infos.as<bool>(true);
        if (const YAML::Node& infos = node["CollapseConsole"])
            editorSettings->CollapseConsole = infos.as<bool>(false);
        if (const YAML::Node& infos = node["ScrollToBottom"])
            editorSettings->ScrollToBottom = infos.as<bool>(true);
        editorSettings->CodeEditorPath = node["CodeEditorPath"].as<String>(String());
        if (const YAML::Node& showColliders = node["ShowPhysicsColliders"])
            editorSettings->ShowPhysicsColliders = showColliders.as<bool>(false);
        else if (const YAML::Node& legacyShowColliders = node["ShowPhysicsColliders2D"])
            editorSettings->ShowPhysicsColliders = legacyShowColliders.as<bool>(false);

        if (const YAML::Node& n = node["WireframeMode"])
            editorSettings->WireframeMode = n.as<bool>(false);
        if (const YAML::Node& n = node["ShowRenderingStatistics"])
            editorSettings->ShowRenderingStatistics = n.as<bool>(true);
        if (const YAML::Node& n = node["ShowGrid"])
            editorSettings->ShowGrid = n.as<bool>(true);
        if (const YAML::Node& n = node["ShowGridAxes"])
            editorSettings->ShowGridAxes = n.as<bool>(true);
        if (const YAML::Node& n = node["GridFineSize"])
            editorSettings->GridFineSize = n.as<float>(1.0f);
        if (const YAML::Node& n = node["GridCoarseSize"])
            editorSettings->GridCoarseSize = n.as<float>(10.0f);
        if (const YAML::Node& n = node["GridLineWidth"])
            editorSettings->GridLineWidth = n.as<float>(0.02f);
        if (const YAML::Node& n = node["GridOpacity"])
            editorSettings->GridOpacity = n.as<float>(0.4f);
        if (const YAML::Node& n = node["ColliderColor"])
            editorSettings->ColliderColor = n.as<glm::vec4>(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

        uint32_t idx = 0;
        for (const auto& project : node["RecentProjects"])
        {
            editorSettings->RecentProjects[idx].ProjectPath = project["Path"].as<String>();
            editorSettings->RecentProjects[idx++].Timestamp = project["Timestamp"].as<std::time_t>();
        }
        return editorSettings;
    }
} // namespace Crowny
