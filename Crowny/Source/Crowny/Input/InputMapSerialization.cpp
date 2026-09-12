#include "cwpch.h"

#include "Crowny/Common/Yaml.h"
#include "Crowny/Input/InputMapSerialization.h"

namespace Crowny
{
    namespace
    {
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

    } // namespace

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
} // namespace Crowny
