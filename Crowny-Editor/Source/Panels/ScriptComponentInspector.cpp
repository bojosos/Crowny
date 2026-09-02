#include "cwepch.h"

// ManagedScriptComponent inspector: draws the reflected script state (fields, dictionaries,
// buttons, conditional attributes, OnValueChanged callbacks), plus the new-script creation
// and script catalog synchronisation used by the entity inspector's Add Component popup.

#include "Panels/ScriptComponentInspector.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/CodeEditor.h"
#include "Panels/ScriptInspectorModel.h"
#include "Panels/ScriptInspectorPath.h"
#include "Panels/ScriptInspectorProgressBar.h"
#include "Panels/ScriptInspectorSearch.h"
#include "UI/Properties.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <charconv>
#include <cmath>
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

        bool ScriptValueTruthy(const ScriptValue& value)
        {
            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                return false;
            case ScriptValueKind::Boolean:
                return value.BooleanValue;
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum:
                return value.SignedValue != 0;
            case ScriptValueKind::UnsignedInteger:
                return value.UnsignedValue != 0;
            case ScriptValueKind::Float:
                return value.FloatingValue != 0.0;
            case ScriptValueKind::Decimal:
            case ScriptValueKind::String:
                return !value.StringValue.empty();
            case ScriptValueKind::Entity:
            case ScriptValueKind::Component:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid:
                return value.ReferenceValue != UUID::EMPTY;
            default:
                return true;
            }
        }

        bool ScriptConditionEquals(const ScriptValue& actual, const ScriptValue& expected)
        {
            if (actual.Kind == ScriptValueKind::Null || expected.Kind == ScriptValueKind::Null)
                return actual.Kind == expected.Kind;
            if (actual.Kind == expected.Kind)
                return actual == expected;
            const bool actualSigned = actual.Kind == ScriptValueKind::SignedInteger || actual.Kind == ScriptValueKind::Enum;
            const bool expectedSigned = expected.Kind == ScriptValueKind::SignedInteger || expected.Kind == ScriptValueKind::Enum;
            const bool actualUnsigned = actual.Kind == ScriptValueKind::UnsignedInteger;
            const bool expectedUnsigned = expected.Kind == ScriptValueKind::UnsignedInteger;
            if (actualSigned && expectedSigned)
                return actual.SignedValue == expected.SignedValue;
            if (actualSigned && expectedUnsigned)
                return actual.SignedValue >= 0 && static_cast<uint64_t>(actual.SignedValue) == expected.UnsignedValue;
            if (actualUnsigned && expectedSigned)
                return expected.SignedValue >= 0 && actual.UnsignedValue == static_cast<uint64_t>(expected.SignedValue);
            if (actualUnsigned && expectedUnsigned)
                return actual.UnsignedValue == expected.UnsignedValue;
            if (actual.Kind == ScriptValueKind::Float && expectedSigned)
                return actual.FloatingValue == static_cast<double>(expected.SignedValue);
            if (actualSigned && expected.Kind == ScriptValueKind::Float)
                return static_cast<double>(actual.SignedValue) == expected.FloatingValue;
            if (actual.Kind == ScriptValueKind::Float && expectedUnsigned)
                return actual.FloatingValue == static_cast<double>(expected.UnsignedValue);
            if (actualUnsigned && expected.Kind == ScriptValueKind::Float)
                return static_cast<double>(actual.UnsignedValue) == expected.FloatingValue;
            return false;
        }

        struct ScriptFieldConditionState
        {
            bool Visible = true;
            bool Enabled = true;
            bool AnimateVisibility = false;
            bool HasVisibilityCondition = false;
        };

        ScriptFieldConditionState EvaluateConditions(const ScriptFieldSchema& field, const ScriptValue& fieldValue,
                                                      const ScriptValue& stateRoot)
        {
            ScriptFieldConditionState state;
            const ScriptConditionalSettings* settings = field.Attributes.Get<ScriptConditionalSettings>();
            if (settings == nullptr)
                return state;
            const ScriptConditionalSettings* resolved = fieldValue.Attributes.Get<ScriptConditionalSettings>();
            for (size_t index = 0; index < settings->Rules.size(); ++index)
            {
                const ScriptConditionRule& rule = settings->Rules[index];
                bool result = false;
                const auto conditionValue = stateRoot.Members.find(rule.Condition);
                if (conditionValue != stateRoot.Members.end())
                    result = rule.HasValue ? ScriptConditionEquals(conditionValue->second, rule.Value)
                                           : ScriptValueTruthy(conditionValue->second);
                else if (resolved != nullptr && index < resolved->Rules.size() && resolved->Rules[index].HasResolvedResult)
                    result = resolved->Rules[index].ResolvedResult;
                switch (rule.Effect)
                {
                case ScriptConditionEffect::Show:
                    state.Visible &= result;
                    state.AnimateVisibility = state.HasVisibilityCondition ? state.AnimateVisibility && rule.Animate : rule.Animate;
                    state.HasVisibilityCondition = true;
                    break;
                case ScriptConditionEffect::Hide:
                    state.Visible &= !result;
                    state.AnimateVisibility = state.HasVisibilityCondition ? state.AnimateVisibility && rule.Animate : rule.Animate;
                    state.HasVisibilityCondition = true;
                    break;
                case ScriptConditionEffect::Enable:
                    state.Enabled &= result;
                    break;
                case ScriptConditionEffect::Disable:
                    state.Enabled &= !result;
                    break;
                }
            }
            return state;
        }

        struct ScriptValueDrawContext
        {
            bool ReadOnly = false;
            const ScriptInspectorAttributeSet* Attributes = nullptr;
            const ScriptCatalog* Catalog = nullptr;
            const ScriptValue* StateRoot = nullptr;

            ScriptValueDrawContext ForChild() const
            {
                ScriptValueDrawContext child = *this;
                child.Attributes = nullptr;
                return child;
            }
        };

        template <typename T, size_t Capacity = 256> class InspectorUiStateCache
        {
        public:
            InspectorUiStateCache() { m_Entries.reserve(Capacity); }

            T& Get(ImGuiID id)
            {
                auto [entry, inserted] = m_Entries.try_emplace(id);
                entry->second.LastUse = ++m_Stamp;
                if (inserted && m_Entries.size() > Capacity)
                {
                    auto victim = m_Entries.end();
                    for (auto candidate = m_Entries.begin(); candidate != m_Entries.end(); ++candidate)
                    {
                        if (candidate->first != id &&
                            (victim == m_Entries.end() || candidate->second.LastUse < victim->second.LastUse))
                            victim = candidate;
                    }
                    if (victim != m_Entries.end())
                        m_Entries.erase(victim);
                }
                return entry->second.Value;
            }

        private:
            struct Entry
            {
                T Value;
                uint64_t LastUse = 0;
            };

            UnorderedMap<ImGuiID, Entry> m_Entries;
            uint64_t m_Stamp = 0;
        };

        struct ButtonUiState
        {
            bool Initialized = false;
            uint64_t ManifestHash = 0;
            bool Expanded = false;
            Vector<ScriptValue> Arguments;
            String Caption;
            ScriptValue Result;
            bool HasResult = false;
            String Error;
        };

        struct ValueChangedUiState
        {
            bool Initialized = false;
            uint64_t Signature = 0;
            uint64_t UndoRedoVersion = 0;
        };

        struct ConditionalUiState
        {
            bool Initialized = false;
            float Visibility = 1.0f;
        };

        enum class ValueChangedTrigger
        {
            Edit,
            Initialize,
            UndoRedo
        };

        struct ValueChangedRequest
        {
            const ScriptFieldSchema* Field = nullptr;
            ValueChangedTrigger Trigger = ValueChangedTrigger::Edit;
            bool ChildChange = false;
        };

        uint64_t ValueChangedSignature(const ScriptFieldSchema& field, const ScriptOnValueChangedSettings& settings)
        {
            uint64_t signature = field.StableId ^ 14695981039346656037ull;
            for (const ScriptValueChangedAction& action : settings.Actions)
            {
                const uint64_t flags = static_cast<uint64_t>(action.IncludeChildren) |
                                       static_cast<uint64_t>(action.InvokeOnInitialize) << 1u |
                                       static_cast<uint64_t>(action.InvokeOnUndoRedo) << 2u |
                                       static_cast<uint64_t>(action.PassValue) << 3u;
                signature ^= action.MethodId + 0x9e3779b97f4a7c15ull + (signature << 6u) + (signature >> 2u) + flags;
            }
            return signature;
        }

        bool DrawScriptValue(const char* label, ScriptValue& value, ScriptValueDrawContext context = {});

        struct DictionaryUiState
        {
            bool Initialized = false;
            bool HasLayoutOverride = false;
            ScriptDictionaryLayout Layout = ScriptDictionaryLayout::TwoColumns;
            uint32_t Revision = 0;
            String PreferenceKey;
            Vector<size_t> ExpandedKeys;
            Vector<size_t> ExpandedValues;
        };

        bool TryGetDictionaryEntry(ScriptValue& entry, ScriptValue*& key, ScriptValue*& value)
        {
            if (entry.Kind != ScriptValueKind::Object)
                return false;
            const auto keyMember = entry.Members.find("Key");
            const auto valueMember = entry.Members.find("Value");
            if (keyMember == entry.Members.end() || valueMember == entry.Members.end())
                return false;
            key = &keyMember->second;
            value = &valueMember->second;
            return true;
        }

        const char* ScriptValueSummary(const ScriptValue& value)
        {
            thread_local char buffer[128];
            const auto formatNumber = [&](auto number) {
                const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, number);
                if (result.ec != std::errc{})
                    return "";
                *result.ptr = '\0';
                return static_cast<const char*>(buffer);
            };
            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                return "null";
            case ScriptValueKind::Boolean:
                return value.BooleanValue ? "true" : "false";
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum:
                return formatNumber(value.SignedValue);
            case ScriptValueKind::UnsignedInteger:
                return formatNumber(value.UnsignedValue);
            case ScriptValueKind::Float:
                return formatNumber(value.FloatingValue);
            case ScriptValueKind::Decimal:
            case ScriptValueKind::String:
                return value.StringValue.empty() ? "empty" : value.StringValue.c_str();
            case ScriptValueKind::Entity:
            case ScriptValueKind::Component:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid:
                if (value.ReferenceValue == UUID::EMPTY)
                    return "None";
                ImFormatString(buffer, sizeof(buffer), "%s", value.ReferenceValue.ToTextBuffer().data());
                return buffer;
            case ScriptValueKind::Object:
                ImFormatString(buffer, sizeof(buffer), "%zu members", value.Members.size());
                return buffer;
            case ScriptValueKind::Array:
            case ScriptValueKind::List:
            case ScriptValueKind::Dictionary:
                ImFormatString(buffer, sizeof(buffer), "%zu items", value.Elements.size());
                return buffer;
            case ScriptValueKind::Vector2:
                return "Vector2";
            case ScriptValueKind::Vector3:
                return "Vector3";
            case ScriptValueKind::Vector4:
                return "Vector4";
            case ScriptValueKind::Color:
                return "Color";
            case ScriptValueKind::Quaternion:
                return "Quaternion";
            case ScriptValueKind::Matrix4:
                return "Matrix4";
            }
            return "";
        }

        bool DrawInlineScriptValue(ScriptValue& value, bool readOnly)
        {
            UI::ScopedDisable disabled(readOnly);
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool changed = false;
            switch (value.Kind)
            {
            case ScriptValueKind::Boolean:
                changed = ImGui::Checkbox("##value", &value.BooleanValue);
                break;
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum:
                changed = ImGui::InputScalar("##value", ImGuiDataType_S64, &value.SignedValue);
                break;
            case ScriptValueKind::UnsignedInteger:
                changed = ImGui::InputScalar("##value", ImGuiDataType_U64, &value.UnsignedValue);
                break;
            case ScriptValueKind::Float:
                changed = ImGui::InputDouble("##value", &value.FloatingValue);
                break;
            case ScriptValueKind::Decimal:
            case ScriptValueKind::String:
                changed = ImGui::InputText("##value", &value.StringValue);
                break;
            case ScriptValueKind::Vector2:
                changed = ImGui::DragFloat2("##value", glm::value_ptr(value.VectorValue));
                break;
            case ScriptValueKind::Vector3:
                changed = ImGui::DragFloat3("##value", glm::value_ptr(value.VectorValue));
                break;
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Quaternion:
                changed = ImGui::DragFloat4("##value", glm::value_ptr(value.VectorValue));
                break;
            case ScriptValueKind::Color:
                if (const ScriptColorUsageSettings* colorUsage = value.Attributes.Get<ScriptColorUsageSettings>())
                    changed = ImGui::ColorEdit4("##value", glm::value_ptr(value.VectorValue),
                                                UI::ColorEditFlags(colorUsage->ShowAlpha, colorUsage->Hdr));
                else
                    changed = ImGui::ColorEdit4("##value", glm::value_ptr(value.VectorValue));
                break;
            case ScriptValueKind::Null:
                ImGui::TextDisabled("null");
                break;
            default:
                ImGui::TextDisabled("%s", ScriptValueSummary(value));
                break;
            }
            UndoRedo::Get().OnItemInteract(changed);
            return changed;
        }

        bool DrawEnumButtons(const char* label, ScriptValue& value, const ScriptEnumButtonsSettings& settings)
        {
            uint64_t rawValue = static_cast<uint64_t>(value.SignedValue);
            if (!UI::EnumButtons(label, settings.Options, rawValue, settings.IsFlags))
                return false;
            value.SignedValue = static_cast<int64_t>(rawValue);
            value.EnumUnsigned = settings.IsUnsigned;
            return true;
        }

        bool DrawPath(const char* label, ScriptValue& value, const ScriptPathSettings& settings, const ScriptValue& stateRoot)
        {
            const Path& projectRoot = Editor::Get().GetProjectPath();
            const bool changed = UI::PropertyFilepathLazy(label, value.StringValue, [&]() {
                FileDialogOptions options;
                options.Type = settings.Kind == ScriptPathKind::File ? FileDialogType::OpenFile : FileDialogType::OpenFolder;
                options.Title = settings.Kind == ScriptPathKind::File ? "Select file" : "Select folder";
                options.InitialDirectory = ScriptInspectorPath::InitialDirectory(value.StringValue, settings, stateRoot, projectRoot);
                options.Filters = ScriptInspectorPath::Filters(settings, stateRoot);
                return options;
            });
            if (changed)
                value.StringValue = ScriptInspectorPath::Normalize(value.StringValue, settings, stateRoot, projectRoot);

            if (settings.RequireExistingPath && !ScriptInspectorPath::Exists(value.StringValue, settings, stateRoot, projectRoot))
            {
                UI::Pre("");
                ImGui::TextColored(ImVec4(0.95f, 0.32f, 0.32f, 1.0f),
                                   settings.Kind == ScriptPathKind::File ? "File does not exist" : "Folder does not exist");
                UI::Post();
            }
            return changed;
        }

        bool DrawProgressBar(const char* label, ScriptValue& value, const ScriptProgressBarSettings& settings, const ScriptValue& stateRoot)
        {
            double numericValue = 0.0;
            if (!ScriptInspectorProgressBar::TryReadNumber(value, numericValue))
                return false;

            double min = 0.0;
            double max = 0.0;
            ScriptInspectorProgressBar::ResolveBounds(settings, stateRoot, min, max);
            const glm::vec4 fill = ScriptInspectorProgressBar::ResolveColor(settings.ColorGetter, stateRoot, glm::vec4(settings.Color, 1.0f));
            const glm::vec4 background =
              ScriptInspectorProgressBar::ResolveColor(settings.BackgroundColorGetter, stateRoot, glm::vec4(0.16f, 0.16f, 0.16f, 1.0f));

            UI::Pre(label);
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const ImVec2 size(std::max(1.0f, ImGui::GetContentRegionAvail().x), static_cast<float>(std::clamp(settings.Height, 1u, 256u)));
            ImGui::InvisibleButton("##progressBar", size, ImGuiButtonFlags_MouseButtonLeft);

            bool changed = false;
            if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && max > min)
            {
                const double positionFraction = std::clamp((ImGui::GetIO().MousePos.x - position.x) / size.x, 0.0f, 1.0f);
                changed = ScriptInspectorProgressBar::TryWriteNumber(value, min + positionFraction * (max - min));
                ScriptInspectorProgressBar::TryReadNumber(value, numericValue);
            }
            UndoRedo::Get().OnItemInteract(changed);

            const float fraction = ScriptInspectorProgressBar::Fraction(numericValue, min, max);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 end(position.x + size.x, position.y + size.y);
            drawList->AddRectFilled(position, end, ImGui::ColorConvertFloat4ToU32(ImVec4(background.r, background.g, background.b, background.a)),
                                    ImGui::GetStyle().FrameRounding);
            const ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(fill.r, fill.g, fill.b, fill.a));
            const double segmentRange = max - min;
            const uint32_t segmentCount =
              settings.Segmented && segmentRange > 1.0 && segmentRange <= 100.0 ? static_cast<uint32_t>(std::round(segmentRange)) : 0;
            if (segmentCount > 1 && segmentCount <= 100)
            {
                const float segmentWidth = size.x / static_cast<float>(segmentCount);
                const uint32_t filledSegments = static_cast<uint32_t>(std::ceil(fraction * static_cast<float>(segmentCount)));
                for (uint32_t segment = 0; segment < filledSegments; ++segment)
                {
                    const float left = position.x + segmentWidth * static_cast<float>(segment) + (segment == 0 ? 0.0f : 1.0f);
                    const float right = position.x + segmentWidth * static_cast<float>(segment + 1) - 1.0f;
                    if (right > left)
                        drawList->AddRectFilled(ImVec2(left, position.y), ImVec2(right, end.y), fillColor, ImGui::GetStyle().FrameRounding);
                }
            }
            else if (fraction > 0.0f)
            {
                drawList->AddRectFilled(position, ImVec2(position.x + size.x * fraction, end.y), fillColor, ImGui::GetStyle().FrameRounding);
            }
            drawList->AddRect(position, end, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);

            if (settings.DrawValueLabel)
            {
                char valueLabelBuffer[64];
                const StringView valueLabel =
                  ScriptInspectorProgressBar::ResolveLabelView(settings, stateRoot, value, valueLabelBuffer, sizeof(valueLabelBuffer));
                const ImVec2 textSize = ImGui::CalcTextSize(valueLabel.data(), valueLabel.data() + valueLabel.size());
                float textX = position.x + 4.0f;
                if (settings.ValueLabelAlignment == ScriptProgressBarLabelAlignment::Center)
                    textX = position.x + (size.x - textSize.x) * 0.5f;
                else if (settings.ValueLabelAlignment == ScriptProgressBarLabelAlignment::Right)
                    textX = end.x - textSize.x - 4.0f;
                const float textY = position.y + (size.y - textSize.y) * 0.5f;
                drawList->PushClipRect(position, end, true);
                drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), valueLabel.data(),
                                  valueLabel.data() + valueLabel.size());
                drawList->PopClipRect();
            }
            UI::Post();
            return changed;
        }

        String& DrawSearchField()
        {
            static InspectorUiStateCache<String> queries;
            String& query = queries.Get(ImGui::GetID("SearchableFilter"));
            UI::Pre("Search");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID("SearchableFilter");
            UIUtils::SearchWidget(query, "Search...");
            ImGui::PopID();
            UI::Post();
            return query;
        }

        bool DrawChildren(const ScriptValue& parent)
        {
            return parent.Kind == ScriptValueKind::Object || parent.Kind == ScriptValueKind::Array || parent.Kind == ScriptValueKind::List ||
                   parent.Kind == ScriptValueKind::Dictionary;
        }

        bool DrawDictionaryTwoColumns(ScriptValue& dictionary, const ScriptDictionaryDisplaySettings& settings,
                                      const ScriptValueDrawContext& context, const String* query,
                                      const ScriptSearchSettings* searchSettings, DictionaryUiState& state,
                                      size_t& visibleEntries)
        {
            bool changed = false;
            state.ExpandedKeys.clear();
            state.ExpandedValues.clear();
            UI::Pre("");
            ImGui::PushID(static_cast<int>(state.Revision));
            const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendX;
            if (ImGui::BeginTable("##dictionaryTable", 2, flags))
            {
                ImGui::TableSetupColumn(settings.KeyLabel.c_str(), ImGuiTableColumnFlags_WidthStretch, settings.KeyColumnFraction);
                ImGui::TableSetupColumn(settings.ValueLabel.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f - settings.KeyColumnFraction);
                ImGui::TableHeadersRow();
                for (size_t index = 0; index < dictionary.Elements.size(); ++index)
                {
                    ScriptValue& entry = dictionary.Elements[index];
                    char entryLabel[32];
                    ImFormatString(entryLabel, sizeof(entryLabel), "Entry %zu", index);
                    if (query != nullptr && searchSettings != nullptr &&
                        !ScriptInspectorSearch::Matches(entryLabel, entry, *query, *searchSettings))
                        continue;
                    ScriptValue* key = nullptr;
                    ScriptValue* value = nullptr;
                    if (!TryGetDictionaryEntry(entry, key, value))
                        continue;
                    ++visibleEntries;
                    ImGui::TableNextRow();
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID("Key");
                    if (DrawChildren(*key))
                    {
                        const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", ScriptValueSummary(*key));
                        if (open)
                        {
                            state.ExpandedKeys.push_back(index);
                            ImGui::TreePop();
                        }
                    }
                    else
                        changed |= DrawInlineScriptValue(*key, context.ReadOnly);
                    ImGui::PopID();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID("Value");
                    if (DrawChildren(*value))
                    {
                        const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", ScriptValueSummary(*value));
                        if (open)
                        {
                            state.ExpandedValues.push_back(index);
                            ImGui::TreePop();
                        }
                    }
                    else
                        changed |= DrawInlineScriptValue(*value, context.ReadOnly);
                    ImGui::PopID();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::PopID();
            UI::Post();

            for (const size_t index : state.ExpandedKeys)
            {
                ScriptValue* key = nullptr;
                ScriptValue* value = nullptr;
                if (!TryGetDictionaryEntry(dictionary.Elements[index], key, value))
                    continue;
                ImGui::PushID(static_cast<int>(index));
                ImGui::Indent(12.0f);
                changed |= DrawScriptValue(settings.KeyLabel.c_str(), *key, context.ForChild());
                ImGui::Unindent(12.0f);
                ImGui::PopID();
            }
            for (const size_t index : state.ExpandedValues)
            {
                ScriptValue* key = nullptr;
                ScriptValue* value = nullptr;
                if (!TryGetDictionaryEntry(dictionary.Elements[index], key, value))
                    continue;
                ImGui::PushID(static_cast<int>(index));
                ImGui::Indent(12.0f);
                changed |= DrawScriptValue(settings.ValueLabel.c_str(), *value, context.ForChild());
                ImGui::Unindent(12.0f);
                ImGui::PopID();
            }
            return changed;
        }

        bool DrawDictionaryOneColumn(ScriptValue& dictionary, const ScriptDictionaryDisplaySettings& settings,
                                     const ScriptValueDrawContext& context, const String* query,
                                     const ScriptSearchSettings* searchSettings, ScriptDictionaryLayout layout,
                                     size_t& visibleEntries)
        {
            bool changed = false;
            for (size_t index = 0; index < dictionary.Elements.size(); ++index)
            {
                ScriptValue& entry = dictionary.Elements[index];
                char entryLabel[32];
                ImFormatString(entryLabel, sizeof(entryLabel), "Entry %zu", index);
                if (query != nullptr && searchSettings != nullptr &&
                    !ScriptInspectorSearch::Matches(entryLabel, entry, *query, *searchSettings))
                    continue;
                ScriptValue* key = nullptr;
                ScriptValue* value = nullptr;
                if (!TryGetDictionaryEntry(entry, key, value))
                    continue;
                ++visibleEntries;
                ImGui::PushID(static_cast<int>(index));
                if (layout == ScriptDictionaryLayout::OneColumnWithValueFoldout)
                {
                    UI::Pre(entryLabel);
                    const bool open = ImGui::TreeNodeEx("##entry", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", ScriptValueSummary(*key));
                    UI::Post();
                    if (open)
                    {
                        ImGui::Indent(12.0f);
                        changed |= DrawScriptValue(settings.KeyLabel.c_str(), *key, context.ForChild());
                        changed |= DrawScriptValue(settings.ValueLabel.c_str(), *value, context.ForChild());
                        ImGui::Unindent(12.0f);
                        ImGui::TreePop();
                    }
                }
                else
                {
                    UI::Pre(entryLabel);
                    ImGui::TextUnformatted(ScriptValueSummary(*key));
                    UI::Post();
                    ImGui::Indent(12.0f);
                    changed |= DrawScriptValue(settings.KeyLabel.c_str(), *key, context.ForChild());
                    changed |= DrawScriptValue(settings.ValueLabel.c_str(), *value, context.ForChild());
                    ImGui::Unindent(12.0f);
                }
                ImGui::PopID();
            }
            return changed;
        }

        bool DrawDictionary(const char* label, ScriptValue& value, const ScriptValueDrawContext& context,
                            const ScriptInspectorAttributeSet& attributes)
        {
            static const ScriptDictionaryDisplaySettings defaults;
            static InspectorUiStateCache<DictionaryUiState> states;
            const ScriptDictionaryDisplaySettings* settings = attributes.Get<ScriptDictionaryDisplaySettings>();
            if (settings == nullptr && context.Catalog != nullptr && value.DeclaredType.IsValid())
                settings = context.Catalog->FindDictionaryDisplay(value.DeclaredType);
            if (settings == nullptr)
                settings = &defaults;

            const ImGuiID dictionaryId = ImGui::GetID("##dictionaryDisplay");
            DictionaryUiState& state = states.Get(dictionaryId);
            const Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
            if (!state.Initialized)
            {
                state.Initialized = true;
                state.PreferenceKey = std::to_string(dictionaryId);
                const auto savedLayout = editorSettings->DictionaryLayouts.find(state.PreferenceKey);
                if (savedLayout != editorSettings->DictionaryLayouts.end() &&
                    savedLayout->second <= static_cast<uint32_t>(ScriptDictionaryLayout::OneColumnWithValueVisible))
                {
                    state.HasLayoutOverride = true;
                    state.Layout = static_cast<ScriptDictionaryLayout>(savedLayout->second);
                }
                const auto savedRevision = editorSettings->DictionaryLayoutRevisions.find(state.PreferenceKey);
                if (savedRevision != editorSettings->DictionaryLayoutRevisions.end())
                    state.Revision = savedRevision->second;
            }
            const String& preferenceKey = state.PreferenceKey;
            ScriptDictionaryLayout layout = state.HasLayoutOverride ? state.Layout : settings->Layout;
            UI::Pre(label);
            const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth, "%zu item%s", value.Elements.size(),
                                                value.Elements.size() == 1 ? "" : "s");
            if (ImGui::BeginPopupContextItem("##dictionaryDisplayContext"))
            {
                if (ImGui::MenuItem("Two columns", nullptr, layout == ScriptDictionaryLayout::TwoColumns))
                {
                    state.HasLayoutOverride = true;
                    state.Layout = ScriptDictionaryLayout::TwoColumns;
                    editorSettings->DictionaryLayouts[preferenceKey] = static_cast<uint32_t>(state.Layout);
                }
                if (ImGui::MenuItem("One column with value foldout", nullptr, layout == ScriptDictionaryLayout::OneColumnWithValueFoldout))
                {
                    state.HasLayoutOverride = true;
                    state.Layout = ScriptDictionaryLayout::OneColumnWithValueFoldout;
                    editorSettings->DictionaryLayouts[preferenceKey] = static_cast<uint32_t>(state.Layout);
                }
                if (ImGui::MenuItem("One column with value visible", nullptr, layout == ScriptDictionaryLayout::OneColumnWithValueVisible))
                {
                    state.HasLayoutOverride = true;
                    state.Layout = ScriptDictionaryLayout::OneColumnWithValueVisible;
                    editorSettings->DictionaryLayouts[preferenceKey] = static_cast<uint32_t>(state.Layout);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset to defaults"))
                {
                    state.HasLayoutOverride = false;
                    ++state.Revision;
                    editorSettings->DictionaryLayouts.erase(preferenceKey);
                    editorSettings->DictionaryLayoutRevisions[preferenceKey] = state.Revision;
                }
                ImGui::EndPopup();
                layout = state.HasLayoutOverride ? state.Layout : settings->Layout;
            }
            UI::Post();
            if (!open)
                return false;

            ImGui::Indent(12.0f);
            const ScriptSearchSettings* searchSettings = attributes.Get<ScriptSearchSettings>();
            const String* query = searchSettings != nullptr ? &DrawSearchField() : nullptr;
            size_t visibleEntries = 0;
            bool changed = false;
            if (layout == ScriptDictionaryLayout::TwoColumns)
                changed = DrawDictionaryTwoColumns(value, *settings, context, query, searchSettings, state, visibleEntries);
            else
                changed = DrawDictionaryOneColumn(value, *settings, context, query, searchSettings, layout, visibleEntries);
            if (query != nullptr && !query->empty() && visibleEntries == 0)
            {
                UI::Pre("");
                ImGui::TextDisabled("No matches");
                UI::Post();
            }
            ImGui::Unindent(12.0f);
            ImGui::TreePop();
            return changed;
        }

        bool DrawStructuredValue(const char* label, ScriptValue& value, const ScriptValueDrawContext& context,
                                 const ScriptInspectorAttributeSet& attributes)
        {
            UI::Pre(label);
            const bool open =
              ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth, "%zu item%s",
                                value.Kind == ScriptValueKind::Object ? value.Members.size() : value.Elements.size(),
                                (value.Kind == ScriptValueKind::Object ? value.Members.size() : value.Elements.size()) == 1 ? "" : "s");
            UI::Post();
            if (!open)
                return false;

            bool changed = false;
            ImGui::Indent(12.0f);
            const ScriptSearchSettings* searchSettings = attributes.Get<ScriptSearchSettings>();
            const ScriptEnumButtonsSettings* enumButtonsSettings = attributes.Get<ScriptEnumButtonsSettings>();
            const String* query = searchSettings != nullptr ? &DrawSearchField() : nullptr;
            size_t visibleChildren = 0;
            if (value.Kind == ScriptValueKind::Object)
            {
                for (auto& [name, member] : value.Members)
                {
                    if (query != nullptr && !ScriptInspectorSearch::Matches(name, member, *query, *searchSettings))
                        continue;
                    ++visibleChildren;
                    ImGui::PushID(name.c_str());
                    changed |= DrawScriptValue(name.c_str(), member, context.ForChild());
                    ImGui::PopID();
                }
            }
            else
            {
                for (size_t index = 0; index < value.Elements.size(); ++index)
                {
                    char elementName[32];
                    ImFormatString(elementName, sizeof(elementName), "Element %zu", index);
                    if (query != nullptr && !ScriptInspectorSearch::Matches(elementName, value.Elements[index], *query, *searchSettings))
                        continue;
                    ++visibleChildren;
                    ImGui::PushID(static_cast<int>(index));
                    if (enumButtonsSettings != nullptr && value.Elements[index].Kind == ScriptValueKind::Enum)
                        changed |= DrawEnumButtons(elementName, value.Elements[index], *enumButtonsSettings);
                    else
                        changed |= DrawScriptValue(elementName, value.Elements[index], context.ForChild());
                    ImGui::PopID();
                }
            }
            if (query != nullptr && !query->empty() && visibleChildren == 0)
            {
                UI::Pre("");
                ImGui::TextDisabled("No matches");
                UI::Post();
            }
            ImGui::Unindent(12.0f);
            ImGui::TreePop();
            return changed;
        }

        bool DrawScriptValue(const char* label, ScriptValue& value, ScriptValueDrawContext context)
        {
            const ScriptInspectorAttributeSet& attributes = context.Attributes != nullptr ? *context.Attributes : value.Attributes;
            const ScriptLabelSettings* labelSettings = attributes.Get<ScriptLabelSettings>();
            const char* displayLabel = labelSettings != nullptr ? labelSettings->Text.c_str() : label;
            const ScriptTooltipSettings* tooltip = attributes.Get<ScriptTooltipSettings>();
            UI::ScopedPropertyTooltip propertyTooltip(tooltip != nullptr ? StringView(tooltip->Text) : StringView());
            UI::ScopedDisable disabled(context.ReadOnly);
            if (value.Kind == ScriptValueKind::Dictionary)
                return DrawDictionary(displayLabel, value, context, attributes);
            if (DrawChildren(value))
                return DrawStructuredValue(displayLabel, value, context, attributes);

            if (const ScriptProgressBarSettings* progressBar = attributes.Get<ScriptProgressBarSettings>();
                progressBar != nullptr && context.StateRoot != nullptr && value.Kind != ScriptValueKind::Null)
                return DrawProgressBar(displayLabel, value, *progressBar, *context.StateRoot);
            if (const ScriptPathSettings* path = attributes.Get<ScriptPathSettings>();
                path != nullptr && context.StateRoot != nullptr && value.Kind == ScriptValueKind::String)
                return DrawPath(displayLabel, value, *path, *context.StateRoot);
            if (const ScriptMultilineSettings* multiline = attributes.Get<ScriptMultilineSettings>();
                multiline != nullptr && value.Kind == ScriptValueKind::String)
                return UI::PropertyMultiline(displayLabel, value.StringValue, multiline->Lines);
            if (const ScriptEnumButtonsSettings* enumButtons = attributes.Get<ScriptEnumButtonsSettings>();
                enumButtons != nullptr && value.Kind == ScriptValueKind::Enum)
                return DrawEnumButtons(displayLabel, value, *enumButtons);

            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                UI::Pre(displayLabel);
                ImGui::TextDisabled("null");
                UI::Post();
                return false;
            case ScriptValueKind::Boolean:
                return UI::Property(displayLabel, value.BooleanValue);
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum:
                return UI::Property(displayLabel, value.SignedValue);
            case ScriptValueKind::UnsignedInteger:
                return UI::Property(displayLabel, value.UnsignedValue);
            case ScriptValueKind::Float:
                return UI::Property(displayLabel, value.FloatingValue);
            case ScriptValueKind::String:
                return UI::Property(displayLabel, value.StringValue);
            case ScriptValueKind::Vector2: {
                glm::vec2 edited(value.VectorValue);
                if (!UI::Property(displayLabel, edited))
                    return false;
                value.VectorValue = glm::vec4(edited, 0.0f, 0.0f);
                return true;
            }
            case ScriptValueKind::Vector3: {
                glm::vec3 edited(value.VectorValue);
                if (!UI::Property(displayLabel, edited))
                    return false;
                value.VectorValue = glm::vec4(edited, 0.0f);
                return true;
            }
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Quaternion:
                return UI::Property(displayLabel, value.VectorValue);
            case ScriptValueKind::Color:
                if (const ScriptColorUsageSettings* colorUsage = attributes.Get<ScriptColorUsageSettings>())
                    return UI::PropertyColor(displayLabel, value.VectorValue, colorUsage->ShowAlpha, colorUsage->Hdr);
                return UI::PropertyColor(displayLabel, value.VectorValue);
            case ScriptValueKind::Matrix4: {
                bool changed = false;
                UI::Pre(displayLabel);
                const bool open = ImGui::TreeNodeEx("##matrix", ImGuiTreeNodeFlags_SpanAvailWidth, "4 x 4");
                UI::Post();
                if (open)
                {
                    ImGui::Indent(12.0f);
                    for (uint32_t row = 0; row < 4; ++row)
                    {
                        glm::vec4 rowValue(value.MatrixValue[0][row], value.MatrixValue[1][row], value.MatrixValue[2][row],
                                           value.MatrixValue[3][row]);
                        char rowLabel[16];
                        ImFormatString(rowLabel, sizeof(rowLabel), "Row %u", row);
                        if (UI::Property(rowLabel, rowValue))
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
            case ScriptValueKind::Uuid: {
                const UUID::TextBuffer reference = value.ReferenceValue.ToTextBuffer();
                UI::Pre(displayLabel);
                ImGui::TextUnformatted(value.ReferenceValue == UUID::EMPTY ? "None" : reference.data());
                UI::Post();
                return false;
            }
            case ScriptValueKind::Array:
            case ScriptValueKind::List:
            case ScriptValueKind::Dictionary:
            case ScriptValueKind::Object:
                break;
            }
            return false;
        }

        bool DrawScriptButtons(const ScriptTypeSchema& schema, const ScriptCatalog& catalog, ManagedScripting& managed,
                               ManagedScript& script, ScriptState& state)
        {
            static InspectorUiStateCache<ButtonUiState, 512> cache;
            bool dirty = false;
            for (const ScriptMethodSchema& method : schema.Methods)
            {
                const ScriptButtonSettings* settings = method.Attributes.Get<ScriptButtonSettings>();
                if (settings == nullptr)
                    continue;

                ImGui::PushID(static_cast<int>(method.StableId ^ (method.StableId >> 32u)));
                ButtonUiState& uiState = cache.Get(ImGui::GetID("ButtonState"));
                if (!uiState.Initialized || uiState.ManifestHash != catalog.ManifestHash)
                {
                    uiState = {};
                    uiState.Initialized = true;
                    uiState.ManifestHash = catalog.ManifestHash;
                    uiState.Expanded = settings->Expanded;
                    uiState.Caption = settings->Name;
                    if (!settings->Icon.empty())
                    {
                        uiState.Caption = settings->IconAlignment == ScriptButtonIconAlignment::Left
                                            ? settings->Icon + "  " + settings->Name
                                            : settings->Name + "  " + settings->Icon;
                    }
                    uiState.Arguments.reserve(method.Parameters.size());
                    for (const ScriptMethodParameterSchema& parameter : method.Parameters)
                    {
                        ScriptValue value = parameter.HasDefaultValue ? parameter.DefaultValue : DefaultValue(parameter.ValueKind);
                        value.Kind = parameter.ValueKind;
                        value.DeclaredType = parameter.DeclaredType;
                        uiState.Arguments.push_back(std::move(value));
                    }
                }

                const bool hasDetails = (settings->DisplayParameters && !method.Parameters.empty()) ||
                                        (settings->DrawResult && uiState.HasResult);
                const bool drawBox = settings->Style == ScriptButtonStyle::Box ||
                                     (settings->Style == ScriptButtonStyle::CompactBox && uiState.Expanded && hasDetails);
                if (drawBox)
                {
                    UI::Pre("");
                    ImGui::Separator();
                    UI::Post();
                }

                if (settings->DisplayParameters && !method.Parameters.empty() && uiState.Expanded)
                {
                    for (size_t index = 0; index < method.Parameters.size(); ++index)
                    {
                        const ScriptMethodParameterSchema& parameter = method.Parameters[index];
                        ImGui::PushID(static_cast<int>(index));
                        ScriptValueDrawContext context;
                        context.Catalog = &catalog;
                        context.StateRoot = &state.Root;
                        DrawScriptValue(parameter.Name.c_str(), uiState.Arguments[index], context);
                        ImGui::PopID();
                    }
                }

                UI::Pre("");
                if (hasDetails)
                {
                    if (ImGui::ArrowButton("##expand", uiState.Expanded ? ImGuiDir_Down : ImGuiDir_Right))
                        uiState.Expanded = !uiState.Expanded;
                    ImGui::SameLine();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(settings->ButtonAlignment, 0.5f));
                const float height = settings->ButtonHeight > 0 ? static_cast<float>(settings->ButtonHeight) : 0.0f;
                const float width = settings->Stretch
                                      ? ImGui::GetContentRegionAvail().x
                                      : ImGui::CalcTextSize(uiState.Caption.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const bool clicked = ImGui::Button(uiState.Caption.c_str(), ImVec2(width, height));
                ImGui::PopStyleVar();
                UI::Post();

                if (clicked)
                {
                    uiState.Error.clear();
                    if (!script.GetRuntimeHandle().IsValid())
                        uiState.Error = "The script has no live managed instance.";
                    else
                    {
                        ScriptInvocationResult invocation = managed.InvokeButton(script.GetRuntimeHandle(), method.StableId, uiState.Arguments);
                        if (!invocation.Result.Succeeded)
                        {
                            for (const ManagedDiagnostic& diagnostic : invocation.Result.Diagnostics)
                            {
                                if (!uiState.Error.empty())
                                    uiState.Error += "\n";
                                uiState.Error += diagnostic.Message;
                            }
                            if (uiState.Error.empty())
                                uiState.Error = "The managed button invocation failed.";
                        }
                        else
                        {
                            uiState.HasResult = invocation.HasReturnValue;
                            uiState.Result = std::move(invocation.ReturnValue);
                            ScriptStateResult captured = managed.CaptureState(script.GetRuntimeHandle());
                            if (captured.Result.Succeeded)
                            {
                                state = std::move(captured.State);
                                if (settings->DirtyOnClick)
                                {
                                    UndoRedo::Get().OnItemInteract(true);
                                    dirty = true;
                                }
                            }
                            else
                                uiState.Error = "The script ran, but its updated state could not be captured.";
                        }
                    }
                }

                if (!uiState.Error.empty())
                {
                    UI::Pre("");
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
                    ImGui::TextWrapped("%s", uiState.Error.c_str());
                    ImGui::PopStyleColor();
                    UI::Post();
                }
                if (settings->DrawResult && uiState.HasResult && uiState.Expanded)
                {
                    ScriptValueDrawContext context;
                    context.ReadOnly = true;
                    context.Catalog = &catalog;
                    context.StateRoot = &state.Root;
                    DrawScriptValue("Result", uiState.Result, context);
                }
                if (drawBox)
                {
                    UI::Pre("");
                    ImGui::Separator();
                    UI::Post();
                }
                ImGui::PopID();
            }
            return dirty;
        }

        bool InvokeValueChangedActions(ManagedScripting& managed, ManagedScript& script, const ScriptState& state,
                                       const Vector<ValueChangedRequest>& requests, bool includeEdits)
        {
            if (!script.GetRuntimeHandle().IsValid() || state.Root.Kind != ScriptValueKind::Object)
                return false;

            bool invoked = false;
            for (const ValueChangedRequest& request : requests)
            {
                if (request.Field == nullptr || (request.Trigger == ValueChangedTrigger::Edit && !includeEdits))
                    continue;
                const ScriptOnValueChangedSettings* settings = request.Field->Attributes.Get<ScriptOnValueChangedSettings>();
                if (settings == nullptr)
                    continue;
                const auto fieldValue = state.Root.Members.find(request.Field->Name);
                for (const ScriptValueChangedAction& action : settings->Actions)
                {
                    if ((request.Trigger == ValueChangedTrigger::Initialize && !action.InvokeOnInitialize) ||
                        (request.Trigger == ValueChangedTrigger::UndoRedo && !action.InvokeOnUndoRedo) ||
                        (request.Trigger == ValueChangedTrigger::Edit && request.ChildChange && !action.IncludeChildren))
                        continue;

                    Vector<ScriptValue> arguments;
                    if (action.PassValue)
                    {
                        if (fieldValue == state.Root.Members.end())
                            continue;
                        arguments.push_back(fieldValue->second);
                    }
                    ScriptInvocationResult result = managed.InvokeButton(script.GetRuntimeHandle(), action.MethodId, arguments);
                    if (!result.Result.Succeeded)
                    {
                        if (result.Result.Diagnostics.empty())
                            CW_ENGINE_ERROR("Managed OnValueChanged action '{}' failed.", action.Action);
                        for (const ManagedDiagnostic& diagnostic : result.Result.Diagnostics)
                            CW_ENGINE_ERROR("Managed OnValueChanged action '{}' failed [{}]: {}", action.Action, diagnostic.Code,
                                            diagnostic.Message);
                        continue;
                    }
                    invoked = true;
                }
            }
            if (invoked)
                ScriptRuntime::CaptureState(script);
            return invoked;
        }

        bool DrawScriptState(const ScriptTypeSchema& schema, const ScriptCatalog& catalog, ManagedScripting& managed,
                             ManagedScript& script, ScriptState& state, Vector<ValueChangedRequest>& valueChangedRequests)
        {
            static InspectorUiStateCache<ValueChangedUiState, 1024> valueChangedStates;
            static InspectorUiStateCache<ConditionalUiState, 1024> conditionalStates;
            if (state.Root.Kind == ScriptValueKind::Null)
                state.Root = ScriptValue::Object({}, schema.Identity);
            if (state.Root.Kind != ScriptValueKind::Object)
                return false;

            for (const ScriptFieldSchema& field : schema.Fields)
            {
                if ((field.Flags & ScriptSchemaFieldFlags::Inspectable) != ScriptSchemaFieldFlags::None)
                    state.Root.Members.try_emplace(field.Name, DefaultValue(field.ValueKind));
            }

            bool changed = false;
            const ScriptSearchSettings* schemaSearch = schema.Attributes.Get<ScriptSearchSettings>();
            const String* query = schemaSearch != nullptr ? &DrawSearchField() : nullptr;
            size_t visibleFields = 0;
            for (const ScriptFieldSchema& field : schema.Fields)
            {
                if ((field.Flags & ScriptSchemaFieldFlags::Inspectable) == ScriptSchemaFieldFlags::None)
                    continue;
                ScriptValue& value = state.Root.Members.try_emplace(field.Name, DefaultValue(field.ValueKind)).first->second;
                ImGui::PushID(field.Name.c_str());
                const ScriptOnValueChangedSettings* valueChanged = field.Attributes.Get<ScriptOnValueChangedSettings>();
                if (valueChanged != nullptr)
                {
                    ValueChangedUiState& uiState = valueChangedStates.Get(ImGui::GetID("##onValueChanged"));
                    const uint64_t signature = ValueChangedSignature(field, *valueChanged);
                    if (uiState.Signature != signature)
                    {
                        uiState.Signature = signature;
                        uiState.Initialized = false;
                        uiState.UndoRedoVersion = UndoRedo::Get().GetActionAppliedVersion();
                    }
                    if (!uiState.Initialized)
                    {
                        uiState.Initialized = true;
                        uiState.UndoRedoVersion = UndoRedo::Get().GetActionAppliedVersion();
                        if (std::any_of(valueChanged->Actions.begin(), valueChanged->Actions.end(),
                                        [](const ScriptValueChangedAction& action) { return action.InvokeOnInitialize; }))
                            valueChangedRequests.push_back({ &field, ValueChangedTrigger::Initialize, false });
                    }
                    const uint64_t undoRedoVersion = UndoRedo::Get().GetActionAppliedVersion();
                    if (uiState.UndoRedoVersion != undoRedoVersion)
                    {
                        uiState.UndoRedoVersion = undoRedoVersion;
                        if (std::any_of(valueChanged->Actions.begin(), valueChanged->Actions.end(),
                                        [](const ScriptValueChangedAction& action) { return action.InvokeOnUndoRedo; }))
                            valueChangedRequests.push_back({ &field, ValueChangedTrigger::UndoRedo, false });
                    }
                }

                const ScriptFieldConditionState condition = EvaluateConditions(field, value, state.Root);
                ConditionalUiState& conditionalUi = conditionalStates.Get(ImGui::GetID("##conditional"));
                if (!conditionalUi.Initialized)
                {
                    conditionalUi.Initialized = true;
                    conditionalUi.Visibility = condition.Visible ? 1.0f : 0.0f;
                }
                else if (!condition.AnimateVisibility)
                    conditionalUi.Visibility = condition.Visible ? 1.0f : 0.0f;
                else
                {
                    const float direction = condition.Visible ? 1.0f : -1.0f;
                    conditionalUi.Visibility = std::clamp(conditionalUi.Visibility + direction * ImGui::GetIO().DeltaTime * 8.0f,
                                                          0.0f, 1.0f);
                }
                if (!condition.Visible && conditionalUi.Visibility <= 0.0f)
                {
                    ImGui::PopID();
                    continue;
                }
                if (query != nullptr &&
                    !ScriptInspectorSearch::Matches(field.Name, value, *query, *schemaSearch, field.ValueKind, &field.DeclaredType))
                {
                    ImGui::PopID();
                    continue;
                }
                ++visibleFields;
                const bool readOnly = (field.Flags & ScriptSchemaFieldFlags::ReadOnly) != ScriptSchemaFieldFlags::None;
                ScriptValueDrawContext context;
                context.ReadOnly = readOnly || !condition.Enabled || !condition.Visible;
                context.Attributes = &field.Attributes;
                context.Catalog = &catalog;
                context.StateRoot = &state.Root;
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * conditionalUi.Visibility);
                const bool fieldChanged = DrawScriptValue(field.Name.c_str(), value, context);
                ImGui::PopStyleVar();
                changed |= fieldChanged;
                if (fieldChanged && valueChanged != nullptr)
                    valueChangedRequests.push_back({ &field, ValueChangedTrigger::Edit, DrawChildren(value) });
                ImGui::PopID();
            }
            if (query != nullptr && !query->empty() && visibleFields == 0)
            {
                UI::Pre("");
                ImGui::TextDisabled("No matches");
                UI::Post();
            }
            changed |= DrawScriptButtons(schema, catalog, managed, script, state);
            return changed;
        }
    } // namespace

    template <> void ComponentEditorWidget<ManagedScriptComponent>(Entity entity)
    {
        ManagedScripting* managed = Application::TryGet()->GetRuntime().GetManagedScripting();
        ManagedScriptComponent& scriptComponent = entity.GetComponent<ManagedScriptComponent>();
        for (uint32_t index = 0; index < scriptComponent.Scripts.size();)
        {
            ManagedScript& script = scriptComponent.Scripts[index];
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Button("-"))
            {
                const ScriptTypeIdentity identity = script.GetTypeIdentity();
                UndoRedo::Get().OnItemInteract(true);
                entity.GetScene()->RemoveScriptComponent(entity, identity);
                ImGui::PopID();
                if (!entity.HasComponent<ManagedScriptComponent>())
                    return;
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

                const ScriptTypeSchema* schema = managed != nullptr ? managed->GetScriptCatalog().FindType(script.GetTypeIdentity()) : nullptr;
                if (schema == nullptr)
                {
                    UI::Pre("Status");
                    ImGui::TextDisabled("Script type unavailable; serialized data is retained");
                    UI::Post();
                }
                else
                {
                    ScriptInspectorModel model(script);
                    Vector<ValueChangedRequest> valueChangedRequests;
                    const bool changed =
                      DrawScriptState(*schema, managed->GetScriptCatalog(), *managed, script, model.GetState(), valueChangedRequests);
                    const bool committed = !changed || model.Commit();
                    if (committed && !valueChangedRequests.empty())
                        InvokeValueChangedActions(*managed, script, model.GetState(), valueChangedRequests, changed);
                }

                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(30.0f);
            }
            ImGui::PopID();
            ++index;
        }
    }


    template <> SelectionComponentChange ComponentSelectionAddAction<ManagedScriptComponent>(std::span<const Entity>)
    {
        // Scripts are attached per type through the Add Component browser, never as a bare component.
        return {};
    }

    template <> Ref<UndoAction> ComponentRemoveAction<ManagedScriptComponent>(Entity entity)
    {
        ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
        entity.RemoveComponent<ManagedScriptComponent>();
        return CreateRef<ChangeScriptComponentAction>(entity, std::move(snapshot), "Remove scripts");
    }

    bool ScriptComponentInspector::IsValidScriptClassName(const String& value)
    {
        if (value.empty())
            return false;

        const auto isValidFirst = [](unsigned char c) { return std::isalpha(c) != 0 || c == '_'; };
        const auto isValidRest = [](unsigned char c) { return std::isalnum(c) != 0 || c == '_'; };
        return isValidFirst(static_cast<unsigned char>(value.front())) &&
               std::all_of(value.begin() + 1, value.end(), [&](char c) { return isValidRest(static_cast<unsigned char>(c)); });
    }

    void ScriptComponentInspector::SynchronizeScriptCatalog(ComponentMenuModel& menu)
    {
        Application* application = Application::TryGet();
        const ManagedScripting* managed = application != nullptr ? application->GetRuntime().GetManagedScripting() : nullptr;
        const ScriptCatalog* catalog = managed != nullptr && managed->IsStarted() ? &managed->GetScriptCatalog() : nullptr;
        const uint64_t fingerprint = catalog != nullptr ? catalog->ManifestHash : 1;
        if (menu.HasScriptCatalog(fingerprint))
            return;

        Vector<ComponentMenuModel::ScriptEntry> scripts;
        if (catalog != nullptr)
        {
            scripts.reserve(catalog->Types.size());
            for (const ScriptTypeSchema& type : catalog->Types)
                scripts.push_back({ type.Identity.TypeName, true, type.Identity });
        }
        menu.SetScripts(fingerprint, std::move(scripts));
    }

    bool ScriptComponentInspector::CreateNewScript(const Vector<Entity>& entities, const String& className)
    {
        static String s_DefaultScriptContents;
        if (s_DefaultScriptContents.empty())
            s_DefaultScriptContents = EditorAssets::GetDefaultScriptTemplate();

        const String scriptNamespace = Editor::Get().GetProjectPath().filename().string();
        const Path path = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (className + ".cs"), FileNamingScheme::UnderscoreIdx);
        const String generatedClassName = path.stem().string();
        String script = StringUtils::Replace(s_DefaultScriptContents, "#NAMESPACE#", scriptNamespace);
        script = StringUtils::Replace(script, "#CLASSNAME#", generatedClassName);
        if (!FileSystem::WriteTextFile(path, script))
        {
            CW_ENGINE_ERROR("Failed to create managed script '{}'.", path.string());
            return false;
        }
        ProjectLibrary::Get().Refresh(path);

        // The project graph synchronizer batches this with the script reload request.
        CodeEditorManager::Get().NotifyProjectInputChanged(path);

        // Keep the serialized attachment while the new type is absent from the
        // runtime catalog. The next managed reload will create its instances.
        UndoRedo::Get().RegisterAction(
          AddManagedScriptToSelection(entities, ScriptTypeIdentity{ GAME_ASSEMBLY, scriptNamespace, generatedClassName }, false).Action);
        return true;
    }
} // namespace Crowny
