#include "cwepch.h"

#include "Panels/ComponentEditor.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "UI/Properties.h"

#include <imgui.h>

namespace Crowny
{
    namespace
    {
        ScriptValue DefaultValue(ScriptValueKind kind)
        {
            ScriptValue value;
            value.Kind = kind;
            if (kind == ScriptValueKind::Object)
                value = ScriptValue::Object({});
            return value;
        }

        bool DrawScriptValue(const String& label, ScriptValue& value);

        bool DrawChildren(const ScriptValue& parent)
        {
            return parent.Kind == ScriptValueKind::Object || parent.Kind == ScriptValueKind::Array || parent.Kind == ScriptValueKind::List ||
                   parent.Kind == ScriptValueKind::Dictionary;
        }

        bool DrawStructuredValue(const String& label, ScriptValue& value)
        {
            UI::Pre(label.c_str());
            const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth, "%zu item%s",
                                                value.Kind == ScriptValueKind::Object ? value.Members.size() : value.Elements.size(),
                                                (value.Kind == ScriptValueKind::Object ? value.Members.size() : value.Elements.size()) == 1 ? "" : "s");
            UI::Post();
            if (!open)
                return false;

            bool changed = false;
            ImGui::Indent(12.0f);
            if (value.Kind == ScriptValueKind::Object)
            {
                for (auto& [name, member] : value.Members)
                    changed |= DrawScriptValue(name, member);
            }
            else
            {
                for (size_t index = 0; index < value.Elements.size(); ++index)
                {
                    ImGui::PushID(static_cast<int>(index));
                    changed |= DrawScriptValue("Element " + std::to_string(index), value.Elements[index]);
                    ImGui::PopID();
                }
            }
            ImGui::Unindent(12.0f);
            ImGui::TreePop();
            return changed;
        }

        bool DrawScriptValue(const String& label, ScriptValue& value)
        {
            if (DrawChildren(value))
                return DrawStructuredValue(label, value);

            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                UI::Pre(label.c_str());
                ImGui::TextDisabled("null");
                UI::Post();
                return false;
            case ScriptValueKind::Boolean: return UI::Property(label.c_str(), value.BooleanValue);
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum: return UI::Property(label.c_str(), value.SignedValue);
            case ScriptValueKind::UnsignedInteger: return UI::Property(label.c_str(), value.UnsignedValue);
            case ScriptValueKind::Float: return UI::Property(label.c_str(), value.FloatingValue);
            case ScriptValueKind::String: return UI::Property(label.c_str(), value.StringValue);
            case ScriptValueKind::Vector2: {
                glm::vec2 edited(value.VectorValue);
                if (!UI::Property(label.c_str(), edited))
                    return false;
                value.VectorValue = glm::vec4(edited, 0.0f, 0.0f);
                return true;
            }
            case ScriptValueKind::Vector3: {
                glm::vec3 edited(value.VectorValue);
                if (!UI::Property(label.c_str(), edited))
                    return false;
                value.VectorValue = glm::vec4(edited, 0.0f);
                return true;
            }
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Quaternion: return UI::Property(label.c_str(), value.VectorValue);
            case ScriptValueKind::Matrix4: {
                bool changed = false;
                UI::Pre(label.c_str());
                const bool open = ImGui::TreeNodeEx("##matrix", ImGuiTreeNodeFlags_SpanAvailWidth, "4 x 4");
                UI::Post();
                if (open)
                {
                    ImGui::Indent(12.0f);
                    for (uint32_t row = 0; row < 4; ++row)
                    {
                        glm::vec4 rowValue(value.MatrixValue[0][row], value.MatrixValue[1][row], value.MatrixValue[2][row],
                                           value.MatrixValue[3][row]);
                        if (UI::Property(("Row " + std::to_string(row)).c_str(), rowValue))
                        {
                            for (uint32_t column = 0; column < 4; ++column)
                                value.MatrixValue[column][row] = rowValue[column];
                            changed = true;
                        }
                    }
                    ImGui::Unindent(12.0f);
                    ImGui::TreePop();
                }
                return changed;
            }
            case ScriptValueKind::Entity:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid:
                UI::Pre(label.c_str());
                ImGui::TextUnformatted(value.ReferenceValue == UUID::EMPTY ? "None" : value.ReferenceValue.ToString().c_str());
                UI::Post();
                return false;
            case ScriptValueKind::Array:
            case ScriptValueKind::List:
            case ScriptValueKind::Dictionary:
            case ScriptValueKind::Object: break;
            }
            return false;
        }

        bool DrawScriptState(const ScriptTypeSchema& schema, ScriptState& state)
        {
            if (state.Root.Kind == ScriptValueKind::Null)
                state.Root = ScriptValue::Object({}, schema.Identity);
            if (state.Root.Kind != ScriptValueKind::Object)
                return false;

            bool changed = false;
            for (const ScriptFieldSchema& field : schema.Fields)
            {
                if ((field.Flags & ScriptFieldFlags::Inspectable) == ScriptFieldFlags::None)
                    continue;
                ScriptValue& value = state.Root.Members.try_emplace(field.Name, DefaultValue(field.ValueKind)).first->second;
                const bool readOnly = (field.Flags & ScriptFieldFlags::ReadOnly) != ScriptFieldFlags::None;
                if (readOnly)
                    ImGui::BeginDisabled();
                changed |= DrawScriptValue(field.Name, value);
                if (readOnly)
                    ImGui::EndDisabled();
            }
            return changed;
        }
    } // namespace

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity entity)
    {
        ManagedScripting* managed = Application::TryGet()->GetRuntime().GetManagedScripting();
        MonoScriptComponent& scriptComponent = entity.GetComponent<MonoScriptComponent>();
        for (uint32_t index = 0; index < scriptComponent.Scripts.size(); ++index)
        {
            MonoScript& script = scriptComponent.Scripts[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Button("-"))
            {
                const ScriptTypeIdentity identity = script.GetTypeIdentity();
                UndoRedo::Get().OnItemInteract(true);
                entity.GetScene()->RemoveScriptComponent(entity, identity);
                ImGui::PopID();
                if (!entity.HasComponent<MonoScriptComponent>())
                    return;
                --index;
                continue;
            }
            ImGui::SameLine();
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(script.GetTypeIdentity().GetFullName().c_str()))
            {
                ImGui::Indent(30.0f);
                ImGui::PushID("Widget");
                UI::BeginPropertyGrid();
                UI::Pre("Type");
                ImGui::TextUnformatted(script.GetTypeIdentity().GetFullName().c_str());
                UI::Post();

                if (!script.GetRuntimeHandle().IsValid())
                    ScriptRuntime::CreateScript(entity, script, false);
                const ScriptTypeSchema* schema = managed != nullptr ? managed->GetScriptCatalog().FindType(script.GetTypeIdentity()) : nullptr;
                if (schema == nullptr || !script.GetRuntimeHandle().IsValid())
                {
                    UI::Pre("Status");
                    ImGui::TextDisabled("Script type unavailable; serialized data is retained");
                    UI::Post();
                }
                else
                {
                    ScriptState state = ScriptRuntime::CaptureState(script);
                    if (DrawScriptState(*schema, state))
                        ScriptRuntime::ApplyState(script, state);
                }

                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(30.0f);
            }
            ImGui::PopID();
        }
    }
} // namespace Crowny
