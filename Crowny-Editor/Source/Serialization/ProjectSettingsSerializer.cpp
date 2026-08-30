#include "cwepch.h"

#include "Editor/Settings/ProjectSettings.h"
#include "Serialization/ProjectSettingsSerializer.h"

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

        const char* ToString(InputActionType type)
        {
            switch (type)
            {
            case InputActionType::Button:
                return "Button";
            case InputActionType::Axis1D:
                return "Axis1D";
            case InputActionType::Axis2D:
                return "Axis2D";
            }
            return "Button";
        }

        InputActionType ParseActionType(const YAML::Node& node)
        {
            const String value = node.as<String>("Button");
            if (value == "Axis1D")
                return InputActionType::Axis1D;
            if (value == "Axis2D")
                return InputActionType::Axis2D;
            return InputActionType::Button;
        }

        const char* ToString(InputBindingDevice device)
        {
            switch (device)
            {
            case InputBindingDevice::Keyboard:
                return "Keyboard";
            case InputBindingDevice::Mouse:
                return "Mouse";
            case InputBindingDevice::GamepadButton:
                return "GamepadButton";
            case InputBindingDevice::GamepadAxis:
                return "GamepadAxis";
            }
            return "Keyboard";
        }

        InputBindingDevice ParseBindingDevice(const YAML::Node& node)
        {
            const String value = node.as<String>("Keyboard");
            if (value == "Mouse")
                return InputBindingDevice::Mouse;
            if (value == "GamepadButton")
                return InputBindingDevice::GamepadButton;
            if (value == "GamepadAxis")
                return InputBindingDevice::GamepadAxis;
            return InputBindingDevice::Keyboard;
        }

        const char* ToString(InputBindingPart part)
        {
            switch (part)
            {
            case InputBindingPart::Whole:
                return "Whole";
            case InputBindingPart::Positive:
                return "Positive";
            case InputBindingPart::Negative:
                return "Negative";
            case InputBindingPart::Up:
                return "Up";
            case InputBindingPart::Down:
                return "Down";
            case InputBindingPart::Left:
                return "Left";
            case InputBindingPart::Right:
                return "Right";
            case InputBindingPart::X:
                return "X";
            case InputBindingPart::Y:
                return "Y";
            }
            return "Whole";
        }

        InputBindingPart ParseBindingPart(const YAML::Node& node)
        {
            const String value = node.as<String>("Whole");
            if (value == "Positive")
                return InputBindingPart::Positive;
            if (value == "Negative")
                return InputBindingPart::Negative;
            if (value == "Up")
                return InputBindingPart::Up;
            if (value == "Down")
                return InputBindingPart::Down;
            if (value == "Left")
                return InputBindingPart::Left;
            if (value == "Right")
                return InputBindingPart::Right;
            if (value == "X")
                return InputBindingPart::X;
            if (value == "Y")
                return InputBindingPart::Y;
            return InputBindingPart::Whole;
        }

        void SerializeInputMap(const InputMap& inputMap, YAML::Emitter& out)
        {
            out << YAML::Key << "Input" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Version" << YAML::Value << 1;
            out << YAML::Key << "Contexts" << YAML::Value << YAML::BeginSeq;
            for (const InputContext& context : inputMap.GetContexts())
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Id" << YAML::Value << context.Id;
                out << YAML::Key << "Name" << YAML::Value << context.Name;
                out << YAML::Key << "Priority" << YAML::Value << context.Priority;
                out << YAML::Key << "Enabled" << YAML::Value << context.Enabled;
                out << YAML::Key << "ConsumeInput" << YAML::Value << context.ConsumeInput;
                out << YAML::Key << "Actions" << YAML::Value << YAML::BeginSeq;
                for (const InputAction& action : context.Actions)
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "Id" << YAML::Value << action.Id;
                    out << YAML::Key << "Name" << YAML::Value << action.Name;
                    out << YAML::Key << "Type" << YAML::Value << ToString(action.Type);
                    out << YAML::Key << "PressThreshold" << YAML::Value << action.PressThreshold;
                    out << YAML::Key << "Bindings" << YAML::Value << YAML::BeginSeq;
                    for (const InputBinding& binding : action.Bindings)
                    {
                        out << YAML::BeginMap;
                        out << YAML::Key << "Id" << YAML::Value << binding.Id;
                        out << YAML::Key << "Device" << YAML::Value << ToString(binding.Device);
                        out << YAML::Key << "Part" << YAML::Value << ToString(binding.Part);
                        out << YAML::Key << "Code" << YAML::Value << binding.Code;
                        out << YAML::Key << "Gamepad" << YAML::Value << binding.GamepadIndex;
                        out << YAML::Key << "Modifiers" << YAML::Value << static_cast<uint32_t>(static_cast<uint8_t>(binding.Modifiers));
                        out << YAML::Key << "Scale" << YAML::Value << binding.Scale;
                        out << YAML::Key << "DeadZone" << YAML::Value << binding.DeadZone;
                        out << YAML::Key << "Invert" << YAML::Value << binding.Invert;
                        out << YAML::EndMap;
                    }
                    out << YAML::EndSeq;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        InputMap DeserializeInputMap(const YAML::Node& node)
        {
            Vector<InputContext> contexts;
            const YAML::Node contextNodes = node["Contexts"];
            if (!contextNodes || !contextNodes.IsSequence())
                return InputMap();

            for (const YAML::Node& contextNode : contextNodes)
            {
                InputContext context;
                context.Id = contextNode["Id"].as<UUID>(UUID::EMPTY);
                context.Name = contextNode["Name"].as<String>("Context");
                context.Priority = contextNode["Priority"].as<int32_t>(0);
                context.Enabled = contextNode["Enabled"].as<bool>(true);
                context.ConsumeInput = contextNode["ConsumeInput"].as<bool>(true);

                const YAML::Node actionNodes = contextNode["Actions"];
                if (actionNodes && actionNodes.IsSequence())
                {
                    for (const YAML::Node& actionNode : actionNodes)
                    {
                        InputAction action;
                        action.Id = actionNode["Id"].as<UUID>(UUID::EMPTY);
                        action.Name = actionNode["Name"].as<String>("Action");
                        action.Type = ParseActionType(actionNode["Type"]);
                        action.PressThreshold = actionNode["PressThreshold"].as<float>(0.5f);

                        const YAML::Node bindingNodes = actionNode["Bindings"];
                        if (bindingNodes && bindingNodes.IsSequence())
                        {
                            for (const YAML::Node& bindingNode : bindingNodes)
                            {
                                InputBinding binding;
                                binding.Id = bindingNode["Id"].as<UUID>(UUID::EMPTY);
                                binding.Device = ParseBindingDevice(bindingNode["Device"]);
                                binding.Part = ParseBindingPart(bindingNode["Part"]);
                                binding.Code = bindingNode["Code"].as<uint32_t>(Key::Space);
                                binding.GamepadIndex = bindingNode["Gamepad"].as<uint32_t>(0);
                                binding.Modifiers = InputModifiers(static_cast<uint8_t>(bindingNode["Modifiers"].as<uint32_t>(0)));
                                binding.Scale = bindingNode["Scale"].as<float>(1.0f);
                                binding.DeadZone = bindingNode["DeadZone"].as<float>(0.15f);
                                binding.Invert = bindingNode["Invert"].as<bool>(false);
                                action.Bindings.push_back(binding);
                            }
                        }
                        context.Actions.push_back(std::move(action));
                    }
                }
                contexts.push_back(std::move(context));
            }
            return InputMap(std::move(contexts));
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
