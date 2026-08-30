#include "cwepch.h"

#include "Panels/InputSettingsEditor.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    namespace
    {
        struct ControlOption
        {
            String Name;
            uint32_t Code;
        };

        const Vector<ControlOption>& GetKeyOptions()
        {
            static const Vector<ControlOption> options = [] {
                Vector<ControlOption> values = {
                    { "Space", Key::Space },
                    { "Apostrophe", Key::Apostrophe },
                    { "Comma", Key::Comma },
                    { "Minus", Key::Minus },
                    { "Period", Key::Period },
                    { "Slash", Key::Slash },
                    { "Semicolon", Key::Semicolon },
                    { "Equal", Key::Equal },
                    { "Left bracket", Key::LeftBracket },
                    { "Backslash", Key::Backslash },
                    { "Right bracket", Key::RightBracket },
                    { "Grave accent", Key::GraveAccent },
                };
                for (uint32_t code = Key::D0; code <= Key::D9; code++)
                    values.push_back({ String(1, static_cast<char>(code)), code });
                for (uint32_t code = Key::A; code <= Key::Z; code++)
                    values.push_back({ String(1, static_cast<char>(code)), code });

                const Vector<ControlOption> named = {
                    { "Escape", Key::Escape },
                    { "Enter", Key::Enter },
                    { "Tab", Key::Tab },
                    { "Backspace", Key::Backspace },
                    { "Insert", Key::Insert },
                    { "Delete", Key::Delete },
                    { "Right arrow", Key::Right },
                    { "Left arrow", Key::Left },
                    { "Down arrow", Key::Down },
                    { "Up arrow", Key::Up },
                    { "Page up", Key::PageUp },
                    { "Page down", Key::PageDown },
                    { "Home", Key::Home },
                    { "End", Key::End },
                    { "Caps lock", Key::CapsLock },
                    { "Scroll lock", Key::ScrollLock },
                    { "Num lock", Key::NumLock },
                    { "Print screen", Key::PrintScreen },
                    { "Pause", Key::Pause },
                    { "Keypad 0", Key::KP0 },
                    { "Keypad 1", Key::KP1 },
                    { "Keypad 2", Key::KP2 },
                    { "Keypad 3", Key::KP3 },
                    { "Keypad 4", Key::KP4 },
                    { "Keypad 5", Key::KP5 },
                    { "Keypad 6", Key::KP6 },
                    { "Keypad 7", Key::KP7 },
                    { "Keypad 8", Key::KP8 },
                    { "Keypad 9", Key::KP9 },
                    { "Keypad decimal", Key::KPDecimal },
                    { "Keypad divide", Key::KPDivide },
                    { "Keypad multiply", Key::KPMultiply },
                    { "Keypad subtract", Key::KPSubtract },
                    { "Keypad add", Key::KPAdd },
                    { "Keypad enter", Key::KPEnter },
                    { "Keypad equal", Key::KPEqual },
                    { "Left shift", Key::LeftShift },
                    { "Left control", Key::LeftControl },
                    { "Left alt", Key::LeftAlt },
                    { "Left super", Key::LeftSuper },
                    { "Right shift", Key::RightShift },
                    { "Right control", Key::RightControl },
                    { "Right alt", Key::RightAlt },
                    { "Right super", Key::RightSuper },
                    { "Menu", Key::Menu },
                };
                values.insert(values.end(), named.begin(), named.end());
                for (uint32_t code = Key::F1; code <= Key::F25; code++)
                    values.push_back({ "F" + std::to_string(code - Key::F1 + 1), code });
                return values;
            }();
            return options;
        }

        const Vector<ControlOption>& GetMouseOptions()
        {
            static const Vector<ControlOption> options = {
                { "Left button", Mouse::ButtonLeft }, { "Right button", Mouse::ButtonRight }, { "Middle button", Mouse::ButtonMiddle },
                { "Button 4", Mouse::Button3 },       { "Button 5", Mouse::Button4 },         { "Button 6", Mouse::Button5 },
                { "Button 7", Mouse::Button6 },       { "Button 8", Mouse::Button7 },
            };
            return options;
        }

        const Vector<ControlOption>& GetGamepadButtonOptions()
        {
            static const Vector<ControlOption> options = {
                { "A", GamepadButton::A },
                { "B", GamepadButton::B },
                { "X", GamepadButton::X },
                { "Y", GamepadButton::Y },
                { "Left bumper", GamepadButton::LeftBumper },
                { "Right bumper", GamepadButton::RightBumper },
                { "Back", GamepadButton::Back },
                { "Start", GamepadButton::Start },
                { "Guide", GamepadButton::Guide },
                { "Left stick", GamepadButton::LeftThumb },
                { "Right stick", GamepadButton::RightThumb },
                { "D-pad up", GamepadButton::DPadUp },
                { "D-pad right", GamepadButton::DPadRight },
                { "D-pad down", GamepadButton::DPadDown },
                { "D-pad left", GamepadButton::DPadLeft },
            };
            return options;
        }

        const Vector<ControlOption>& GetGamepadAxisOptions()
        {
            static const Vector<ControlOption> options = {
                { "Left stick X", GamepadAxis::LeftX },       { "Left stick Y", GamepadAxis::LeftY },
                { "Right stick X", GamepadAxis::RightX },     { "Right stick Y", GamepadAxis::RightY },
                { "Left trigger", GamepadAxis::LeftTrigger }, { "Right trigger", GamepadAxis::RightTrigger },
            };
            return options;
        }

        const Vector<ControlOption>& GetControlOptions(InputBindingDevice device)
        {
            switch (device)
            {
            case InputBindingDevice::Keyboard:
                return GetKeyOptions();
            case InputBindingDevice::Mouse:
                return GetMouseOptions();
            case InputBindingDevice::GamepadButton:
                return GetGamepadButtonOptions();
            case InputBindingDevice::GamepadAxis:
                return GetGamepadAxisOptions();
            }
            return GetKeyOptions();
        }

        const char* GetControlName(const InputBinding& binding)
        {
            for (const ControlOption& option : GetControlOptions(binding.Device))
            {
                if (option.Code == binding.Code)
                    return option.Name.c_str();
            }
            return "Unknown control";
        }

        bool ControlDropdown(InputBinding& binding)
        {
            bool changed = false;
            if (ImGui::BeginCombo("Control", GetControlName(binding)))
            {
                for (const ControlOption& option : GetControlOptions(binding.Device))
                {
                    const bool selected = binding.Code == option.Code;
                    if (ImGui::Selectable(option.Name.c_str(), selected))
                    {
                        binding.Code = option.Code;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool EnumDropdown(const char* label, int32_t& value, const char* const* labels, int32_t count)
        {
            value = std::clamp(value, 0, count - 1);
            bool changed = false;
            if (ImGui::BeginCombo(label, labels[value]))
            {
                for (int32_t i = 0; i < count; i++)
                {
                    const bool selected = value == i;
                    if (ImGui::Selectable(labels[i], selected))
                    {
                        value = i;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool ModifierCheckbox(const char* label, InputModifiers& modifiers, InputModifierBits bit)
        {
            bool enabled = modifiers.IsSet(bit);
            if (!ImGui::Checkbox(label, &enabled))
                return false;
            if (enabled)
                modifiers.Set(bit);
            else
                modifiers.Unset(bit);
            return true;
        }

        void ResetControlForDevice(InputBinding& binding)
        {
            const Vector<ControlOption>& options = GetControlOptions(binding.Device);
            binding.Code = options.front().Code;
            binding.DeadZone = binding.Device == InputBindingDevice::GamepadAxis ? 0.15f : 0.0f;
        }

        bool RenderBinding(InputBinding& binding, InputActionType actionType)
        {
            bool changed = false;
            static const char* deviceLabels[] = { "Keyboard", "Mouse", "Gamepad button", "Gamepad axis" };
            int32_t device = static_cast<int32_t>(binding.Device);
            if (EnumDropdown("Device", device, deviceLabels, static_cast<int32_t>(std::size(deviceLabels))))
            {
                binding.Device = static_cast<InputBindingDevice>(device);
                ResetControlForDevice(binding);
                changed = true;
            }
            changed |= ControlDropdown(binding);

            if (actionType == InputActionType::Axis1D)
            {
                static const char* partLabels[] = { "Whole axis", "Positive", "Negative" };
                static const InputBindingPart parts[] = { InputBindingPart::Whole, InputBindingPart::Positive, InputBindingPart::Negative };
                int32_t selected = 0;
                for (int32_t i = 0; i < static_cast<int32_t>(std::size(parts)); i++)
                {
                    if (binding.Part == parts[i])
                        selected = i;
                }
                if (EnumDropdown("Composite part", selected, partLabels, static_cast<int32_t>(std::size(partLabels))))
                {
                    binding.Part = parts[selected];
                    changed = true;
                }
            }
            else if (actionType == InputActionType::Axis2D)
            {
                static const char* partLabels[] = { "X axis", "Y axis", "Up", "Down", "Left", "Right" };
                static const InputBindingPart parts[] = { InputBindingPart::X,    InputBindingPart::Y,    InputBindingPart::Up,
                                                          InputBindingPart::Down, InputBindingPart::Left, InputBindingPart::Right };
                int32_t selected = 0;
                for (int32_t i = 0; i < static_cast<int32_t>(std::size(parts)); i++)
                {
                    if (binding.Part == parts[i])
                        selected = i;
                }
                if (EnumDropdown("Composite part", selected, partLabels, static_cast<int32_t>(std::size(partLabels))))
                {
                    binding.Part = parts[selected];
                    changed = true;
                }
            }

            if (binding.Device == InputBindingDevice::Keyboard || binding.Device == InputBindingDevice::Mouse)
            {
                ImGui::TextUnformatted("Modifiers");
                ImGui::SameLine();
                changed |= ModifierCheckbox("Shift", binding.Modifiers, InputModifierBits::Shift);
                ImGui::SameLine();
                changed |= ModifierCheckbox("Control", binding.Modifiers, InputModifierBits::Control);
                ImGui::SameLine();
                changed |= ModifierCheckbox("Alt", binding.Modifiers, InputModifierBits::Alt);
                ImGui::SameLine();
                changed |= ModifierCheckbox("Super", binding.Modifiers, InputModifierBits::Super);
            }

            if (binding.Device == InputBindingDevice::GamepadButton || binding.Device == InputBindingDevice::GamepadAxis)
            {
                int32_t gamepad = static_cast<int32_t>(binding.GamepadIndex);
                if (ImGui::InputInt("Gamepad", &gamepad))
                {
                    binding.GamepadIndex = static_cast<uint32_t>(std::clamp(gamepad, 0, 15));
                    changed = true;
                }
            }
            if (binding.Device == InputBindingDevice::GamepadAxis)
            {
                changed |= ImGui::SliderFloat("Dead zone", &binding.DeadZone, 0.0f, 0.95f, "%.2f");
            }
            changed |= ImGui::DragFloat("Scale", &binding.Scale, 0.05f, 0.0f, 4.0f, "%.2f");
            changed |= ImGui::Checkbox("Invert", &binding.Invert);
            return changed;
        }

        InputBinding MakeKeyBinding(KeyCode key, InputBindingPart part)
        {
            InputBinding binding;
            binding.Id = UuidGenerator::Generate();
            binding.Device = InputBindingDevice::Keyboard;
            binding.Code = key;
            binding.Part = part;
            binding.DeadZone = 0.0f;
            return binding;
        }

        bool HasDuplicateActionName(const InputContext& context, size_t actionIndex)
        {
            if (context.Actions[actionIndex].Name.empty())
                return true;
            for (size_t i = 0; i < context.Actions.size(); i++)
            {
                if (i != actionIndex && context.Actions[i].Name == context.Actions[actionIndex].Name)
                    return true;
            }
            return false;
        }

        bool RenderAction(InputContext& context, size_t actionIndex, bool& removeAction)
        {
            InputAction& action = context.Actions[actionIndex];
            bool changed = false;
            ImGui::PushID(action.Id.ToString().c_str());
            const String header = action.Name.empty() ? "Unnamed action" : action.Name;
            const bool open = ImGui::TreeNodeEx("##Action", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", header.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
                removeAction = true;

            if (open)
            {
                ImGui::SetNextItemWidth(220.0f);
                changed |= ImGui::InputText("Name", &action.Name);
                if (HasDuplicateActionName(context, actionIndex))
                    ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "Action names must be unique in a context.");

                static const char* typeLabels[] = { "Button", "1D axis", "2D axis" };
                int32_t type = static_cast<int32_t>(action.Type);
                if (EnumDropdown("Value type", type, typeLabels, static_cast<int32_t>(std::size(typeLabels))))
                {
                    action.Type = static_cast<InputActionType>(type);
                    for (InputBinding& binding : action.Bindings)
                    {
                        if (action.Type == InputActionType::Button)
                            binding.Part = InputBindingPart::Whole;
                        else if (action.Type == InputActionType::Axis2D && binding.Part == InputBindingPart::Whole)
                            binding.Part = InputBindingPart::X;
                    }
                    changed = true;
                }
                changed |= ImGui::SliderFloat("Press threshold", &action.PressThreshold, 0.01f, 1.0f, "%.2f");

                ImGui::SeparatorText("Bindings");
                size_t removeBinding = action.Bindings.size();
                for (size_t bindingIndex = 0; bindingIndex < action.Bindings.size(); bindingIndex++)
                {
                    InputBinding& binding = action.Bindings[bindingIndex];
                    ImGui::PushID(binding.Id.ToString().c_str());
                    const String bindingLabel = std::to_string(bindingIndex + 1) + ". " + GetControlName(binding);
                    const bool bindingOpen = ImGui::TreeNodeEx("##Binding", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", bindingLabel.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove"))
                        removeBinding = bindingIndex;
                    if (bindingOpen)
                    {
                        changed |= RenderBinding(binding, action.Type);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (removeBinding < action.Bindings.size())
                {
                    action.Bindings.erase(action.Bindings.begin() + static_cast<ptrdiff_t>(removeBinding));
                    changed = true;
                }

                if (ImGui::Button("Add binding"))
                {
                    action.Bindings.push_back(
                      MakeKeyBinding(Key::Space, action.Type == InputActionType::Axis2D ? InputBindingPart::X : InputBindingPart::Whole));
                    changed = true;
                }
                if (action.Type == InputActionType::Axis1D)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Add A/D composite"))
                    {
                        action.Bindings.push_back(MakeKeyBinding(Key::A, InputBindingPart::Negative));
                        action.Bindings.push_back(MakeKeyBinding(Key::D, InputBindingPart::Positive));
                        changed = true;
                    }
                }
                else if (action.Type == InputActionType::Axis2D)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Add WASD composite"))
                    {
                        action.Bindings.push_back(MakeKeyBinding(Key::W, InputBindingPart::Up));
                        action.Bindings.push_back(MakeKeyBinding(Key::S, InputBindingPart::Down));
                        action.Bindings.push_back(MakeKeyBinding(Key::A, InputBindingPart::Left));
                        action.Bindings.push_back(MakeKeyBinding(Key::D, InputBindingPart::Right));
                        changed = true;
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            return changed;
        }
    } // namespace

    bool InputSettingsEditor::Render(InputMap& inputMap)
    {
        return inputMap.EditContexts([](Vector<InputContext>& contexts) {
            bool changed = false;

            ImGui::TextWrapped(
              "Define named actions here, then query them from gameplay code. Higher priorities win when contexts use the same action name.");
            if (ImGui::Button("Add context"))
            {
                InputContext context;
                context.Id = UuidGenerator::Generate();
                context.Name = contexts.empty() ? "Gameplay" : "New context";
                contexts.push_back(std::move(context));
                changed = true;
            }

            size_t removeContext = contexts.size();
            for (size_t contextIndex = 0; contextIndex < contexts.size(); contextIndex++)
            {
                InputContext& context = contexts[contextIndex];
                ImGui::PushID(context.Id.ToString().c_str());
                ImGui::Separator();
                const String header = context.Name.empty() ? "Unnamed context" : context.Name;
                const bool open =
                  ImGui::TreeNodeEx("##Context", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", header.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                    removeContext = contextIndex;

                if (open)
                {
                    ImGui::SetNextItemWidth(220.0f);
                    changed |= ImGui::InputText("Name", &context.Name);
                    changed |= ImGui::InputInt("Priority", &context.Priority);
                    changed |= ImGui::Checkbox("Enabled", &context.Enabled);
                    changed |= ImGui::Checkbox("Consume active bindings", &context.ConsumeInput);
                    ImGui::TextDisabled("Consumed controls do not reach lower-priority contexts.");

                    size_t removeAction = context.Actions.size();
                    for (size_t actionIndex = 0; actionIndex < context.Actions.size(); actionIndex++)
                    {
                        bool remove = false;
                        changed |= RenderAction(context, actionIndex, remove);
                        if (remove)
                            removeAction = actionIndex;
                    }
                    if (removeAction < context.Actions.size())
                    {
                        context.Actions.erase(context.Actions.begin() + static_cast<ptrdiff_t>(removeAction));
                        changed = true;
                    }

                    if (ImGui::Button("Add action"))
                    {
                        InputAction action;
                        action.Id = UuidGenerator::Generate();
                        action.Name = "New action";
                        context.Actions.push_back(std::move(action));
                        changed = true;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (removeContext < contexts.size())
            {
                contexts.erase(contexts.begin() + static_cast<ptrdiff_t>(removeContext));
                changed = true;
            }
            return changed;
        });
    }
} // namespace Crowny
