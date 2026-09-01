#include "cwpch.h"

#include "Crowny/Scripting/Managed/ManagedTypes.h"

#include <cmath>

namespace Crowny
{
    namespace
    {
        bool MatricesEqual(const glm::mat4& lhs, const glm::mat4& rhs)
        {
            for (uint32_t column = 0; column < 4; ++column)
            {
                if (lhs[column] != rhs[column])
                    return false;
            }
            return true;
        }

        bool IsValid(const ScriptSearchSettings& settings)
        {
            const uint32_t options = static_cast<uint32_t>(settings.FilterOptions);
            return (options & ~static_cast<uint32_t>(ScriptSearchFilterOptions::All)) == 0;
        }

        bool IsValid(const ScriptProgressBarSettings& settings)
        {
            const bool validColor = std::isfinite(settings.Color.r) && std::isfinite(settings.Color.g) && std::isfinite(settings.Color.b);
            const bool validAlignment = settings.ValueLabelAlignment >= ScriptProgressBarLabelAlignment::Left &&
                                        settings.ValueLabelAlignment <= ScriptProgressBarLabelAlignment::Right;
            return validColor && validAlignment && std::isfinite(settings.Min) && std::isfinite(settings.Max) && settings.Height > 0;
        }

        bool SupportsProgressBar(ScriptValueKind kind)
        {
            return kind == ScriptValueKind::SignedInteger || kind == ScriptValueKind::UnsignedInteger || kind == ScriptValueKind::Float ||
                   kind == ScriptValueKind::Decimal;
        }

        bool IsValid(const ScriptEnumButtonsSettings& settings)
        {
            Set<String> names;
            for (const ScriptEnumOption& option : settings.Options)
            {
                if (option.Name.empty() || !names.insert(option.Name).second)
                    return false;
            }
            return true;
        }

        bool IsValid(const ScriptDictionaryDisplaySettings& settings)
        {
            return settings.Layout >= ScriptDictionaryLayout::TwoColumns && settings.Layout <= ScriptDictionaryLayout::OneColumnWithValueVisible &&
                   std::isfinite(settings.KeyColumnFraction) && settings.KeyColumnFraction >= 0.01f && settings.KeyColumnFraction <= 0.99f;
        }
    } // namespace

    ScriptValue ScriptValue::Null() { return {}; }

    ScriptValue ScriptValue::Boolean(bool value)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::Boolean;
        result.BooleanValue = value;
        return result;
    }

    ScriptValue ScriptValue::Signed(int64_t value)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::SignedInteger;
        result.SignedValue = value;
        return result;
    }

    ScriptValue ScriptValue::Unsigned(uint64_t value)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::UnsignedInteger;
        result.UnsignedValue = value;
        return result;
    }

    ScriptValue ScriptValue::Float(double value)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::Float;
        result.FloatingValue = value;
        return result;
    }

    ScriptValue ScriptValue::Text(String value)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::String;
        result.StringValue = std::move(value);
        return result;
    }

    ScriptValue ScriptValue::Object(Map<String, ScriptValue> members, ScriptTypeIdentity type)
    {
        ScriptValue result;
        result.Kind = ScriptValueKind::Object;
        result.Members = std::move(members);
        result.DeclaredType = std::move(type);
        return result;
    }

    bool ScriptValue::operator==(const ScriptValue& other) const
    {
        if (Kind != other.Kind || DeclaredType != other.DeclaredType || EnumUnsigned != other.EnumUnsigned)
            return false;

        switch (Kind)
        {
        case ScriptValueKind::Null:
            return true;
        case ScriptValueKind::Boolean:
            return BooleanValue == other.BooleanValue;
        case ScriptValueKind::SignedInteger:
            return SignedValue == other.SignedValue;
        case ScriptValueKind::UnsignedInteger:
            return UnsignedValue == other.UnsignedValue;
        case ScriptValueKind::Float:
            return FloatingValue == other.FloatingValue;
        case ScriptValueKind::Decimal:
        case ScriptValueKind::String:
            return StringValue == other.StringValue;
        case ScriptValueKind::Enum:
            return SignedValue == other.SignedValue;
        case ScriptValueKind::Vector2:
        case ScriptValueKind::Vector3:
        case ScriptValueKind::Vector4:
        case ScriptValueKind::Color:
        case ScriptValueKind::Quaternion:
            return VectorValue == other.VectorValue;
        case ScriptValueKind::Matrix4:
            return MatricesEqual(MatrixValue, other.MatrixValue);
        case ScriptValueKind::Entity:
        case ScriptValueKind::Component:
        case ScriptValueKind::Asset:
        case ScriptValueKind::Uuid:
            return ReferenceValue == other.ReferenceValue;
        case ScriptValueKind::Array:
        case ScriptValueKind::List:
        case ScriptValueKind::Dictionary:
            return Elements == other.Elements;
        case ScriptValueKind::Object:
            return Members == other.Members;
        }
        return false;
    }

    ScriptEvent ScriptEvent::Lifecycle(ScriptEventKind kind, float deltaTime)
    {
        ScriptEvent event;
        event.Kind = kind;
        event.DeltaTime = deltaTime;
        return event;
    }

    const ScriptTypeSchema* ScriptCatalog::FindType(const ScriptTypeIdentity& identity) const
    {
        const auto type = std::find_if(Types.begin(), Types.end(), [&](const ScriptTypeSchema& candidate) { return candidate.Identity == identity; });
        return type != Types.end() ? &*type : nullptr;
    }

    const ScriptDictionaryDisplaySettings* ScriptCatalog::FindDictionaryDisplay(const ScriptTypeIdentity& identity) const
    {
        const auto rule = std::find_if(DictionaryDisplays.begin(), DictionaryDisplays.end(),
                                       [&](const ScriptDictionaryDisplayRule& candidate) { return candidate.TargetType == identity; });
        return rule != DictionaryDisplays.end() ? &rule->Display : nullptr;
    }

    ManagedOperationResult ManagedOperationResult::Success() { return {}; }

    ManagedOperationResult ManagedOperationResult::Failure(String code, String message, ManagedBackendId backend)
    {
        ManagedOperationResult result;
        result.Succeeded = false;
        result.Diagnostics.push_back({ ManagedDiagnosticSeverity::Error, std::move(code), std::move(message), {}, backend, {}, {} });
        return result;
    }

    bool ManagedOperationResult::HasDiagnosticCode(StringView code) const
    {
        return std::any_of(Diagnostics.begin(), Diagnostics.end(), [&](const ManagedDiagnostic& diagnostic) { return diagnostic.Code == code; });
    }

    const char* ToString(ManagedBackendId backend)
    {
        switch (backend)
        {
        case ManagedBackendId::Mono:
            return "Mono";
        case ManagedBackendId::CoreCLR:
            return "CoreCLR";
        case ManagedBackendId::DotNetWasm:
            return "DotNetWasm";
        case ManagedBackendId::NativeAOT:
            return "NativeAOT";
        case ManagedBackendId::GeneratedMetadata:
            return "GeneratedMetadata";
        }
        return "Unknown";
    }

    const char* ToString(ManagedExecutionMode mode)
    {
        switch (mode)
        {
        case ManagedExecutionMode::Interpreter:
            return "Interpreter";
        case ManagedExecutionMode::Jit:
            return "Jit";
        case ManagedExecutionMode::ReadyToRun:
            return "ReadyToRun";
        case ManagedExecutionMode::Aot:
            return "Aot";
        }
        return "Unknown";
    }

    ManagedOperationResult ValidateScriptCatalog(const ScriptCatalog& catalog, ManagedBackendId backend)
    {
        if (catalog.ManifestVersion == 0 || catalog.ManifestHash == 0)
            return ManagedOperationResult::Failure("managed.catalog.manifest_invalid", "The script catalog has no manifest version or content hash.",
                                                   backend);

        Set<uint64_t> typeIds;
        Set<String> identities;
        for (const ScriptDictionaryDisplayRule& rule : catalog.DictionaryDisplays)
        {
            if (!rule.TargetType.IsValid() || !IsValid(rule.Display))
                return ManagedOperationResult::Failure("managed.catalog.dictionary_display_invalid",
                                                       "The script catalog contains an invalid dictionary display rule.", backend);
        }
        for (const ScriptTypeSchema& type : catalog.Types)
        {
            if (!type.Identity.IsValid() || type.StableId == 0 || !typeIds.insert(type.StableId).second)
                return ManagedOperationResult::Failure("managed.catalog.type_invalid", "The script catalog contains an invalid or duplicate type.",
                                                       backend);

            const String key = type.Identity.Assembly + ":" + type.Identity.GetFullName();
            if (!identities.insert(key).second)
                return ManagedOperationResult::Failure("managed.catalog.type_identity_collision",
                                                       "The script catalog contains a colliding type identity.", backend);
            const ScriptSearchSettings* typeSearch = type.Attributes.Get<ScriptSearchSettings>();
            if (typeSearch != nullptr && !IsValid(*typeSearch))
                return ManagedOperationResult::Failure("managed.catalog.type_search_invalid",
                                                       "The script catalog contains invalid type search settings.", backend);

            Set<uint64_t> fieldIds;
            Set<String> fieldNames;
            for (const ScriptFieldSchema& field : type.Fields)
            {
                if (field.StableId == 0 || field.Name.empty() || !fieldIds.insert(field.StableId).second || !fieldNames.insert(field.Name).second)
                    return ManagedOperationResult::Failure("managed.catalog.field_invalid",
                                                           "The script catalog contains an invalid or duplicate field.", backend);
                const ScriptSearchSettings* search = field.Attributes.Get<ScriptSearchSettings>();
                const ScriptProgressBarSettings* progressBar = field.Attributes.Get<ScriptProgressBarSettings>();
                const ScriptEnumButtonsSettings* enumButtons = field.Attributes.Get<ScriptEnumButtonsSettings>();
                const ScriptDictionaryDisplaySettings* dictionaryDisplay =
                  field.Attributes.Get<ScriptDictionaryDisplaySettings>();
                const ScriptConditionalSettings* conditions = field.Attributes.Get<ScriptConditionalSettings>();
                const ScriptOnValueChangedSettings* valueChanged = field.Attributes.Get<ScriptOnValueChangedSettings>();
                if (search != nullptr && !IsValid(*search))
                    return ManagedOperationResult::Failure("managed.catalog.field_search_invalid",
                                                           "The script catalog contains invalid field search settings.", backend);
                if (progressBar != nullptr && (!SupportsProgressBar(field.ValueKind) || !IsValid(*progressBar)))
                    return ManagedOperationResult::Failure("managed.catalog.field_progress_bar_invalid",
                                                           "The script catalog contains invalid field progress bar settings.", backend);
                if ((field.Attributes.Has<ScriptPathSettings>() || field.Attributes.Has<ScriptMultilineSettings>()) &&
                    field.ValueKind != ScriptValueKind::String)
                    return ManagedOperationResult::Failure("managed.catalog.field_string_attribute_invalid",
                                                           "The script catalog applies a string inspector attribute to a non-string field.", backend);
                if (field.Attributes.Has<ScriptColorUsageSettings>() && field.ValueKind != ScriptValueKind::Color)
                    return ManagedOperationResult::Failure("managed.catalog.field_color_usage_invalid",
                                                           "The script catalog applies color usage settings to a non-color field.", backend);
                if (enumButtons != nullptr && field.ValueKind != ScriptValueKind::Enum &&
                    !((field.ValueKind == ScriptValueKind::Array || field.ValueKind == ScriptValueKind::List) &&
                      field.ElementKind == ScriptValueKind::Enum))
                    return ManagedOperationResult::Failure("managed.catalog.field_enum_buttons_invalid",
                                                           "The script catalog applies enum buttons to a non-enum field.", backend);
                if (enumButtons != nullptr && !IsValid(*enumButtons))
                    return ManagedOperationResult::Failure("managed.catalog.field_enum_buttons_invalid",
                                                           "The script catalog contains invalid enum button options.", backend);
                if (dictionaryDisplay != nullptr &&
                    (field.ValueKind != ScriptValueKind::Dictionary || !IsValid(*dictionaryDisplay)))
                    return ManagedOperationResult::Failure("managed.catalog.field_dictionary_display_invalid",
                                                           "The script catalog contains invalid field dictionary display settings.", backend);
                if (conditions != nullptr &&
                    std::any_of(conditions->Rules.begin(), conditions->Rules.end(),
                                [](const ScriptConditionRule& rule) { return rule.Condition.empty(); }))
                    return ManagedOperationResult::Failure("managed.catalog.field_condition_invalid",
                                                           "The script catalog contains an invalid inspector condition.", backend);
                if (valueChanged != nullptr &&
                    std::any_of(valueChanged->Actions.begin(), valueChanged->Actions.end(), [](const ScriptValueChangedAction& action) {
                        return action.Action.empty() || action.MethodId == 0;
                    }))
                    return ManagedOperationResult::Failure("managed.catalog.field_value_changed_invalid",
                                                           "The script catalog contains an invalid value-changed action.", backend);
            }
            Set<uint64_t> methodIds;
            for (const ScriptMethodSchema& method : type.Methods)
            {
                const ScriptButtonSettings* button = method.Attributes.Get<ScriptButtonSettings>();
                if (method.StableId == 0 || method.Name.empty() || !methodIds.insert(method.StableId).second || button == nullptr ||
                    button->Name.empty() || button->ButtonHeight < 0 || button->ButtonAlignment < 0.0f || button->ButtonAlignment > 1.0f)
                    return ManagedOperationResult::Failure("managed.catalog.button_invalid",
                                                           "The script catalog contains invalid or duplicate button metadata.", backend);
                for (const ScriptMethodParameterSchema& parameter : method.Parameters)
                {
                    if (parameter.Name.empty() || parameter.ValueKind == ScriptValueKind::Null ||
                        (parameter.HasDefaultValue && parameter.DefaultValue.Kind != parameter.ValueKind &&
                         parameter.DefaultValue.Kind != ScriptValueKind::Null))
                        return ManagedOperationResult::Failure("managed.catalog.button_parameter_invalid",
                                                               "The script catalog contains an invalid button parameter.", backend);
                }
            }
        }
        return ManagedOperationResult::Success();
    }

    ScriptStateResult NormalizeScriptState(const ScriptState& state, const ScriptTypeSchema& target, ManagedBackendId backend)
    {
        if (state.Identity.IsValid() && state.Identity != target.Identity)
            return { ManagedOperationResult::Failure("managed.script.state_identity_mismatch", "Script state belongs to an incompatible script type.",
                                                     backend),
                     {} };
        if (state.Root.Kind != ScriptValueKind::Null && state.Root.Kind != ScriptValueKind::Object)
            return { ManagedOperationResult::Failure("managed.script.state_root_invalid", "Script state must be represented by an object value.",
                                                     backend),
                     {} };

        ScriptState normalized;
        normalized.Identity = target.Identity;
        normalized.Root = ScriptValue::Object({}, target.Identity);
        for (const ScriptFieldSchema& field : target.Fields)
        {
            if ((field.Flags & ScriptSchemaFieldFlags::Serializable) == ScriptSchemaFieldFlags::None)
                continue;
            const auto value = state.Root.Members.find(field.Name);
            if (value == state.Root.Members.end())
                continue;
            if (value->second.Kind == field.ValueKind || value->second.Kind == ScriptValueKind::Null)
                normalized.Root.Members[field.Name] = value->second;
        }
        return { ManagedOperationResult::Success(), std::move(normalized) };
    }
} // namespace Crowny
