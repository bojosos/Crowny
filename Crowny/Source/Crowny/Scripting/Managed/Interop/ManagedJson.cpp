#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace Crowny
{
    namespace
    {
        uint64_t Hash(StringView value)
        {
            uint64_t hash = 14695981039346656037ull;
            for (const uint8_t byte : value)
                hash = (hash ^ byte) * 1099511628211ull;
            return hash == 0 ? 1 : hash;
        }

        bool ReadString(const rapidjson::Value& object, const char* name, String& output)
        {
            const auto value = object.FindMember(name);
            if (value == object.MemberEnd() || !value->value.IsString())
                return false;
            output.assign(value->value.GetString(), value->value.GetStringLength());
            return true;
        }

        bool ReadIdentity(const rapidjson::Value& value, ScriptTypeIdentity& output)
        {
            return value.IsObject() && ReadString(value, "Assembly", output.Assembly) && ReadString(value, "Namespace", output.Namespace) &&
                   ReadString(value, "TypeName", output.TypeName) && output.IsValid();
        }

        bool ReadSearchSettings(const rapidjson::Value& value, ScriptSearchSettings& output)
        {
            if (!value.IsObject())
                return false;

            const auto filterOptions = value.FindMember("FilterOptions");
            const auto fuzzySearch = value.FindMember("FuzzySearch");
            const auto recursive = value.FindMember("Recursive");
            if (filterOptions == value.MemberEnd() || !filterOptions->value.IsUint() || fuzzySearch == value.MemberEnd() ||
                !fuzzySearch->value.IsBool() || recursive == value.MemberEnd() || !recursive->value.IsBool())
                return false;

            const uint32_t options = filterOptions->value.GetUint();
            const uint32_t knownOptions = static_cast<uint32_t>(ScriptSearchFilterOptions::All);
            if ((options & ~knownOptions) != 0)
                return false;
            output.FilterOptions = static_cast<ScriptSearchFilterOptions>(options);
            output.FuzzySearch = fuzzySearch->value.GetBool();
            output.Recursive = recursive->value.GetBool();
            return true;
        }

        bool ReadProgressBarSettings(const rapidjson::Value& value, ScriptProgressBarSettings& output)
        {
            if (!value.IsObject())
                return false;

            const auto min = value.FindMember("Min");
            const auto max = value.FindMember("Max");
            const auto red = value.FindMember("R");
            const auto green = value.FindMember("G");
            const auto blue = value.FindMember("B");
            const auto height = value.FindMember("Height");
            const auto segmented = value.FindMember("Segmented");
            const auto drawValueLabel = value.FindMember("DrawValueLabel");
            const auto alignment = value.FindMember("ValueLabelAlignment");
            if (min == value.MemberEnd() || !min->value.IsNumber() || max == value.MemberEnd() || !max->value.IsNumber() ||
                red == value.MemberEnd() || !red->value.IsNumber() || green == value.MemberEnd() || !green->value.IsNumber() ||
                blue == value.MemberEnd() || !blue->value.IsNumber() || height == value.MemberEnd() || !height->value.IsUint() ||
                segmented == value.MemberEnd() || !segmented->value.IsBool() || drawValueLabel == value.MemberEnd() ||
                !drawValueLabel->value.IsBool() || alignment == value.MemberEnd() || !alignment->value.IsUint() ||
                alignment->value.GetUint() > static_cast<uint32_t>(ScriptProgressBarLabelAlignment::Right))
                return false;

            if (!ReadString(value, "MinGetter", output.MinGetter) || !ReadString(value, "MaxGetter", output.MaxGetter) ||
                !ReadString(value, "ColorGetter", output.ColorGetter) ||
                !ReadString(value, "BackgroundColorGetter", output.BackgroundColorGetter) ||
                !ReadString(value, "CustomValueStringGetter", output.CustomValueStringGetter))
                return false;
            output.Min = min->value.GetDouble();
            output.Max = max->value.GetDouble();
            output.Color = glm::vec3(static_cast<float>(red->value.GetDouble()), static_cast<float>(green->value.GetDouble()),
                                     static_cast<float>(blue->value.GetDouble()));
            output.Height = height->value.GetUint();
            output.Segmented = segmented->value.GetBool();
            output.DrawValueLabel = drawValueLabel->value.GetBool();
            output.ValueLabelAlignment = static_cast<ScriptProgressBarLabelAlignment>(alignment->value.GetUint());
            return true;
        }

        bool ReadPathSettings(const rapidjson::Value& value, ScriptPathKind kind, ScriptPathSettings& output)
        {
            if (!value.IsObject())
                return false;
            const auto absolutePath = value.FindMember("AbsolutePath");
            const auto requireExistingPath = value.FindMember("RequireExistingPath");
            const auto useBackslashes = value.FindMember("UseBackslashes");
            if (absolutePath == value.MemberEnd() || !absolutePath->value.IsBool() || requireExistingPath == value.MemberEnd() ||
                !requireExistingPath->value.IsBool() || useBackslashes == value.MemberEnd() || !useBackslashes->value.IsBool())
                return false;

            output.Kind = kind;
            if (!ReadString(value, "ParentFolder", output.ParentFolder))
                return false;
            output.AbsolutePath = absolutePath->value.GetBool();
            output.RequireExistingPath = requireExistingPath->value.GetBool();
            output.UseBackslashes = useBackslashes->value.GetBool();
            if (kind == ScriptPathKind::File)
            {
                const auto includeFileExtension = value.FindMember("IncludeFileExtension");
                if (!ReadString(value, "Extensions", output.Extensions) || includeFileExtension == value.MemberEnd() ||
                    !includeFileExtension->value.IsBool())
                    return false;
                output.IncludeFileExtension = includeFileExtension->value.GetBool();
            }
            return true;
        }

        bool ReadMultilineSettings(const rapidjson::Value& value, ScriptMultilineSettings& output)
        {
            if (!value.IsObject())
                return false;
            const auto lines = value.FindMember("Lines");
            if (lines == value.MemberEnd() || !lines->value.IsInt())
                return false;
            output.Lines = lines->value.GetInt();
            return true;
        }

        bool ReadColorUsageSettings(const rapidjson::Value& value, ScriptColorUsageSettings& output)
        {
            if (!value.IsObject())
                return false;
            const auto showAlpha = value.FindMember("ShowAlpha");
            const auto hdr = value.FindMember("Hdr");
            if (showAlpha == value.MemberEnd() || !showAlpha->value.IsBool() || hdr == value.MemberEnd() || !hdr->value.IsBool())
                return false;
            output.ShowAlpha = showAlpha->value.GetBool();
            output.Hdr = hdr->value.GetBool();
            return true;
        }

        bool ReadEnumButtonsSettings(const rapidjson::Value& value, ScriptEnumButtonsSettings& output)
        {
            if (!value.IsObject())
                return false;
            const auto isFlags = value.FindMember("IsFlags");
            const auto isUnsigned = value.FindMember("IsUnsigned");
            const auto includeObsolete = value.FindMember("IncludeObsolete");
            const auto options = value.FindMember("Options");
            if (isFlags == value.MemberEnd() || !isFlags->value.IsBool() || isUnsigned == value.MemberEnd() || !isUnsigned->value.IsBool() ||
                includeObsolete == value.MemberEnd() || !includeObsolete->value.IsBool() || options == value.MemberEnd() || !options->value.IsArray())
                return false;

            output.IsFlags = isFlags->value.GetBool();
            output.IsUnsigned = isUnsigned->value.GetBool();
            output.IncludeObsolete = includeObsolete->value.GetBool();
            for (const rapidjson::Value& optionValue : options->value.GetArray())
            {
                if (!optionValue.IsObject())
                    return false;
                const auto name = optionValue.FindMember("Name");
                const auto option = optionValue.FindMember("Value");
                if (name == optionValue.MemberEnd() || !name->value.IsString() || option == optionValue.MemberEnd() || !option->value.IsUint64())
                    return false;
                output.Options.push_back({ String(name->value.GetString(), name->value.GetStringLength()), option->value.GetUint64() });
            }
            return true;
        }

        bool ReadDictionaryDisplaySettings(const rapidjson::Value& value, ScriptDictionaryDisplaySettings& output)
        {
            if (!value.IsObject())
                return false;
            const auto layout = value.FindMember("Layout");
            const auto fraction = value.FindMember("KeyColumnFraction");
            if (layout == value.MemberEnd() || !layout->value.IsUint() ||
                layout->value.GetUint() > static_cast<uint32_t>(ScriptDictionaryLayout::OneColumnWithValueVisible) || fraction == value.MemberEnd() ||
                !fraction->value.IsNumber() || !ReadString(value, "KeyLabel", output.KeyLabel) || !ReadString(value, "ValueLabel", output.ValueLabel))
                return false;
            output.Layout = static_cast<ScriptDictionaryLayout>(layout->value.GetUint());
            output.KeyColumnFraction = static_cast<float>(fraction->value.GetDouble());
            if (output.KeyLabel.empty())
                output.KeyLabel = "Key";
            if (output.ValueLabel.empty())
                output.ValueLabel = "Value";
            return true;
        }

        bool TryParseKind(StringView value, ScriptValueKind& output);
        ScriptValue ReadValue(const rapidjson::Value& value, ScriptValueKind expectedKind = ScriptValueKind::Null);

        bool ReadInspectorAttributes(const rapidjson::Value& value, ScriptInspectorAttributeSet& output)
        {
            if (!value.IsObject())
                return false;
            if (value.HasMember("Label"))
            {
                ScriptLabelSettings label;
                if (!ReadString(value, "Label", label.Text))
                    return false;
                output.Set(std::move(label));
            }
            if (value.HasMember("Tooltip"))
            {
                ScriptTooltipSettings tooltip;
                if (!ReadString(value, "Tooltip", tooltip.Text))
                    return false;
                output.Set(std::move(tooltip));
            }
            if (value.HasMember("Searchable"))
            {
                ScriptSearchSettings settings;
                if (!ReadSearchSettings(value["Searchable"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("ProgressBar"))
            {
                ScriptProgressBarSettings settings;
                if (!ReadProgressBarSettings(value["ProgressBar"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("FilePath") && value.HasMember("FolderPath"))
                return false;
            if (value.HasMember("FilePath") || value.HasMember("FolderPath"))
            {
                const bool isFile = value.HasMember("FilePath");
                ScriptPathSettings settings;
                if (!ReadPathSettings(value[isFile ? "FilePath" : "FolderPath"],
                                      isFile ? ScriptPathKind::File : ScriptPathKind::Folder, settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("Multiline"))
            {
                ScriptMultilineSettings settings;
                if (!ReadMultilineSettings(value["Multiline"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("ColorUsage"))
            {
                ScriptColorUsageSettings settings;
                if (!ReadColorUsageSettings(value["ColorUsage"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("EnumButtons"))
            {
                ScriptEnumButtonsSettings settings;
                if (!ReadEnumButtonsSettings(value["EnumButtons"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("DictionaryDisplay"))
            {
                ScriptDictionaryDisplaySettings settings;
                if (!ReadDictionaryDisplaySettings(value["DictionaryDisplay"], settings))
                    return false;
                output.Set(std::move(settings));
            }
            if (value.HasMember("Conditions"))
            {
                if (!value["Conditions"].IsArray())
                    return false;
                ScriptConditionalSettings settings;
                for (const rapidjson::Value& conditionValue : value["Conditions"].GetArray())
                {
                    if (!conditionValue.IsObject())
                        return false;
                    ScriptConditionRule rule;
                    const auto effect = conditionValue.FindMember("Effect");
                    const auto animate = conditionValue.FindMember("Animate");
                    const auto hasValue = conditionValue.FindMember("HasValue");
                    String valueKind;
                    if (effect == conditionValue.MemberEnd() || !effect->value.IsUint() ||
                        effect->value.GetUint() > static_cast<uint32_t>(ScriptConditionEffect::Disable) ||
                        animate == conditionValue.MemberEnd() || !animate->value.IsBool() || hasValue == conditionValue.MemberEnd() ||
                        !hasValue->value.IsBool() || !ReadString(conditionValue, "Condition", rule.Condition) ||
                        !ReadString(conditionValue, "ValueKind", valueKind) || !conditionValue.HasMember("Value"))
                        return false;
                    ScriptValueKind kind;
                    if (!TryParseKind(valueKind, kind))
                        return false;
                    rule.Effect = static_cast<ScriptConditionEffect>(effect->value.GetUint());
                    rule.Animate = animate->value.GetBool();
                    rule.HasValue = hasValue->value.GetBool();
                    rule.Value = ReadValue(conditionValue["Value"], kind);
                    if (conditionValue.HasMember("Result"))
                    {
                        if (!conditionValue["Result"].IsBool())
                            return false;
                        rule.HasResolvedResult = true;
                        rule.ResolvedResult = conditionValue["Result"].GetBool();
                    }
                    settings.Rules.push_back(std::move(rule));
                }
                output.Set(std::move(settings));
            }
            if (value.HasMember("OnValueChanged"))
            {
                if (!value["OnValueChanged"].IsArray())
                    return false;
                ScriptOnValueChangedSettings settings;
                for (const rapidjson::Value& actionValue : value["OnValueChanged"].GetArray())
                {
                    if (!actionValue.IsObject())
                        return false;
                    ScriptValueChangedAction action;
                    const auto methodId = actionValue.FindMember("MethodId");
                    const auto includeChildren = actionValue.FindMember("IncludeChildren");
                    const auto invokeOnInitialize = actionValue.FindMember("InvokeOnInitialize");
                    const auto invokeOnUndoRedo = actionValue.FindMember("InvokeOnUndoRedo");
                    const auto passValue = actionValue.FindMember("PassValue");
                    if (!ReadString(actionValue, "Action", action.Action) ||
                        methodId == actionValue.MemberEnd() || !methodId->value.IsUint64() ||
                        includeChildren == actionValue.MemberEnd() || !includeChildren->value.IsBool() ||
                        invokeOnInitialize == actionValue.MemberEnd() || !invokeOnInitialize->value.IsBool() ||
                        invokeOnUndoRedo == actionValue.MemberEnd() || !invokeOnUndoRedo->value.IsBool() ||
                        passValue == actionValue.MemberEnd() || !passValue->value.IsBool())
                        return false;
                    action.MethodId = methodId->value.GetUint64();
                    action.IncludeChildren = includeChildren->value.GetBool();
                    action.InvokeOnInitialize = invokeOnInitialize->value.GetBool();
                    action.InvokeOnUndoRedo = invokeOnUndoRedo->value.GetBool();
                    action.PassValue = passValue->value.GetBool();
                    settings.Actions.push_back(std::move(action));
                }
                output.Set(std::move(settings));
            }
            return true;
        }

        bool InspectorAttributesMatchKind(const ScriptInspectorAttributeSet& attributes, ScriptValueKind kind)
        {
            const bool numeric = kind == ScriptValueKind::SignedInteger || kind == ScriptValueKind::UnsignedInteger ||
                                 kind == ScriptValueKind::Float || kind == ScriptValueKind::Decimal;
            if (attributes.Has<ScriptProgressBarSettings>() && !numeric)
                return false;
            if ((attributes.Has<ScriptPathSettings>() || attributes.Has<ScriptMultilineSettings>()) && kind != ScriptValueKind::String)
                return false;
            if (attributes.Has<ScriptColorUsageSettings>() && kind != ScriptValueKind::Color)
                return false;
            if (attributes.Has<ScriptEnumButtonsSettings>() && kind != ScriptValueKind::Enum && kind != ScriptValueKind::Array &&
                kind != ScriptValueKind::List)
                return false;
            if (attributes.Has<ScriptDictionaryDisplaySettings>() && kind != ScriptValueKind::Dictionary)
                return false;
            return true;
        }

        bool TryParseKind(StringView value, ScriptValueKind& output)
        {
            static const Map<StringView, ScriptValueKind> kinds = {
                { "Null", ScriptValueKind::Null },
                { "Boolean", ScriptValueKind::Boolean },
                { "SignedInteger", ScriptValueKind::SignedInteger },
                { "UnsignedInteger", ScriptValueKind::UnsignedInteger },
                { "Float", ScriptValueKind::Float },
                { "Decimal", ScriptValueKind::Decimal },
                { "String", ScriptValueKind::String },
                { "Enum", ScriptValueKind::Enum },
                { "Vector2", ScriptValueKind::Vector2 },
                { "Vector3", ScriptValueKind::Vector3 },
                { "Vector4", ScriptValueKind::Vector4 },
                { "Color", ScriptValueKind::Color },
                { "Quaternion", ScriptValueKind::Quaternion },
                { "Matrix4", ScriptValueKind::Matrix4 },
                { "Entity", ScriptValueKind::Entity },
                { "Component", ScriptValueKind::Component },
                { "Asset", ScriptValueKind::Asset },
                { "Array", ScriptValueKind::Array },
                { "List", ScriptValueKind::List },
                { "Dictionary", ScriptValueKind::Dictionary },
                { "Object", ScriptValueKind::Object },
                { "Uuid", ScriptValueKind::Uuid },
            };
            const auto kind = kinds.find(value);
            if (kind == kinds.end())
                return false;
            output = kind->second;
            return true;
        }

        const char* KindName(ScriptValueKind kind)
        {
            switch (kind)
            {
            case ScriptValueKind::Null:
                return "Null";
            case ScriptValueKind::Boolean:
                return "Boolean";
            case ScriptValueKind::SignedInteger:
                return "SignedInteger";
            case ScriptValueKind::UnsignedInteger:
                return "UnsignedInteger";
            case ScriptValueKind::Float:
                return "Float";
            case ScriptValueKind::Decimal:
                return "Decimal";
            case ScriptValueKind::String:
                return "String";
            case ScriptValueKind::Enum:
                return "Enum";
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
            case ScriptValueKind::Entity:
                return "Entity";
            case ScriptValueKind::Component:
                return "Component";
            case ScriptValueKind::Asset:
                return "Asset";
            case ScriptValueKind::Array:
                return "Array";
            case ScriptValueKind::List:
                return "List";
            case ScriptValueKind::Dictionary:
                return "Dictionary";
            case ScriptValueKind::Object:
                return "Object";
            case ScriptValueKind::Uuid:
                return "Uuid";
            }
            return "Null";
        }

        ScriptValue ReadValue(const rapidjson::Value& value, ScriptValueKind expectedKind)
        {
            if (value.IsNull())
                return ScriptValue::Null();
            if (expectedKind == ScriptValueKind::Boolean && value.IsBool())
                return ScriptValue::Boolean(value.GetBool());
            if (expectedKind == ScriptValueKind::SignedInteger && value.IsInt64())
            {
                ScriptValue result = ScriptValue::Signed(value.GetInt64());
                result.Kind = expectedKind;
                return result;
            }
            if (expectedKind == ScriptValueKind::Enum && (value.IsInt64() || value.IsUint64()))
            {
                ScriptValue result = ScriptValue::Signed(value.IsUint64() ? static_cast<int64_t>(value.GetUint64()) : value.GetInt64());
                result.Kind = ScriptValueKind::Enum;
                return result;
            }
            if (expectedKind == ScriptValueKind::UnsignedInteger && value.IsUint64())
                return ScriptValue::Unsigned(value.GetUint64());
            if (expectedKind == ScriptValueKind::Float && value.IsNumber())
                return ScriptValue::Float(value.GetDouble());
            if (expectedKind == ScriptValueKind::Decimal && value.IsString())
            {
                ScriptValue result = ScriptValue::Text(String(value.GetString(), value.GetStringLength()));
                result.Kind = ScriptValueKind::Decimal;
                return result;
            }
            if (expectedKind == ScriptValueKind::String && value.IsString())
                return ScriptValue::Text(String(value.GetString(), value.GetStringLength()));
            if ((expectedKind == ScriptValueKind::Entity || expectedKind == ScriptValueKind::Component || expectedKind == ScriptValueKind::Asset ||
                 expectedKind == ScriptValueKind::Uuid) &&
                value.IsString())
            {
                ScriptValue result;
                result.Kind = expectedKind;
                result.ReferenceValue = UUID(String(value.GetString(), value.GetStringLength()));
                return result;
            }
            if ((expectedKind == ScriptValueKind::Vector2 || expectedKind == ScriptValueKind::Vector3 || expectedKind == ScriptValueKind::Vector4 ||
                 expectedKind == ScriptValueKind::Color || expectedKind == ScriptValueKind::Quaternion) &&
                value.IsArray())
            {
                const uint32_t count = expectedKind == ScriptValueKind::Vector2 ? 2 : expectedKind == ScriptValueKind::Vector3 ? 3 : 4;
                if (value.Size() >= count)
                {
                    ScriptValue result;
                    result.Kind = expectedKind;
                    for (uint32_t index = 0; index < count; ++index)
                    {
                        if (!value[index].IsNumber())
                            return ReadValue(value);
                        result.VectorValue[index] = static_cast<float>(value[index].GetDouble());
                    }
                    return result;
                }
            }
            if (expectedKind == ScriptValueKind::Matrix4 && value.IsArray() && value.Size() >= 16)
            {
                ScriptValue result;
                result.Kind = ScriptValueKind::Matrix4;
                for (uint32_t column = 0; column < 4; ++column)
                {
                    for (uint32_t row = 0; row < 4; ++row)
                    {
                        const rapidjson::Value& element = value[column * 4 + row];
                        if (!element.IsNumber())
                            return ReadValue(value);
                        result.MatrixValue[column][row] = static_cast<float>(element.GetDouble());
                    }
                }
                return result;
            }
            if ((expectedKind == ScriptValueKind::Array || expectedKind == ScriptValueKind::List || expectedKind == ScriptValueKind::Dictionary) &&
                value.IsArray())
            {
                ScriptValue result;
                result.Kind = expectedKind;
                for (const rapidjson::Value& element : value.GetArray())
                    result.Elements.push_back(ReadValue(element));
                return result;
            }
            if (value.IsBool())
                return ScriptValue::Boolean(value.GetBool());
            if (value.IsInt64())
                return ScriptValue::Signed(value.GetInt64());
            if (value.IsUint64())
                return ScriptValue::Unsigned(value.GetUint64());
            if (value.IsNumber())
                return ScriptValue::Float(value.GetDouble());
            if (value.IsString())
                return ScriptValue::Text(String(value.GetString(), value.GetStringLength()));
            if (value.IsArray())
            {
                ScriptValue result;
                result.Kind = ScriptValueKind::List;
                for (const rapidjson::Value& element : value.GetArray())
                    result.Elements.push_back(ReadValue(element));
                return result;
            }
            ScriptValue result = ScriptValue::Object({});
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                result.Members.emplace(String(member->name.GetString(), member->name.GetStringLength()), ReadValue(member->value));
            return result;
        }

        ScriptValue ReadValueWithMetadata(const rapidjson::Value& value, const rapidjson::Value* metadata,
                                          ScriptValueKind fallbackKind = ScriptValueKind::Null)
        {
            ScriptValueKind kind = fallbackKind;
            if (metadata != nullptr)
            {
                if (!metadata->IsObject() || !metadata->HasMember("Kind") || !(*metadata)["Kind"].IsString() ||
                    !TryParseKind(StringView((*metadata)["Kind"].GetString(), (*metadata)["Kind"].GetStringLength()), kind))
                    throw std::runtime_error("Managed state contains invalid value metadata");
            }

            ScriptValue result = ReadValue(value, kind);
            if (metadata == nullptr)
                return result;

            if (metadata->HasMember("Assembly") || metadata->HasMember("Namespace") || metadata->HasMember("TypeName"))
            {
                if (!ReadString(*metadata, "Assembly", result.DeclaredType.Assembly) ||
                    !ReadString(*metadata, "Namespace", result.DeclaredType.Namespace) ||
                    !ReadString(*metadata, "TypeName", result.DeclaredType.TypeName))
                    throw std::runtime_error("Managed state contains invalid declared-type metadata");
            }

            if (metadata->HasMember("EnumUnsigned"))
            {
                if (result.Kind != ScriptValueKind::Enum || !(*metadata)["EnumUnsigned"].IsBool())
                    throw std::runtime_error("Managed state contains invalid enum metadata");
                result.EnumUnsigned = (*metadata)["EnumUnsigned"].GetBool();
            }

            if (!ReadInspectorAttributes(*metadata, result.Attributes) || !InspectorAttributesMatchKind(result.Attributes, result.Kind))
                throw std::runtime_error("Managed state contains invalid inspector attribute metadata");

            if (value.IsObject() && metadata->HasMember("Members"))
            {
                const rapidjson::Value& members = (*metadata)["Members"];
                if (!members.IsObject())
                    throw std::runtime_error("Managed state contains invalid object metadata");
                result.Members.clear();
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                {
                    const auto memberMetadata = members.FindMember(member->name.GetString());
                    result.Members.emplace(
                      String(member->name.GetString(), member->name.GetStringLength()),
                      ReadValueWithMetadata(member->value, memberMetadata != members.MemberEnd() ? &memberMetadata->value : nullptr));
                }
            }
            else if (value.IsArray() && metadata->HasMember("Elements"))
            {
                const rapidjson::Value& elements = (*metadata)["Elements"];
                if (!elements.IsArray() || elements.Size() != value.Size())
                    throw std::runtime_error("Managed state contains invalid collection metadata");
                result.Elements.clear();
                result.Elements.reserve(value.Size());
                for (rapidjson::SizeType index = 0; index < value.Size(); ++index)
                    result.Elements.push_back(ReadValueWithMetadata(value[index], &elements[index]));
            }
            return result;
        }

        template <class Writer> void WriteValue(Writer& writer, const ScriptValue& value)
        {
            switch (value.Kind)
            {
            case ScriptValueKind::Null:
                writer.Null();
                break;
            case ScriptValueKind::Boolean:
                writer.Bool(value.BooleanValue);
                break;
            case ScriptValueKind::SignedInteger:
                writer.Int64(value.SignedValue);
                break;
            case ScriptValueKind::Enum:
                if (value.EnumUnsigned)
                    writer.Uint64(static_cast<uint64_t>(value.SignedValue));
                else
                    writer.Int64(value.SignedValue);
                break;
            case ScriptValueKind::UnsignedInteger:
                writer.Uint64(value.UnsignedValue);
                break;
            case ScriptValueKind::Float:
                writer.Double(value.FloatingValue);
                break;
            case ScriptValueKind::Decimal:
            case ScriptValueKind::String:
                writer.String(value.StringValue.c_str(), static_cast<rapidjson::SizeType>(value.StringValue.size()));
                break;
            case ScriptValueKind::Entity:
            case ScriptValueKind::Component:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid: {
                const String uuid = value.ReferenceValue.ToString();
                writer.String(uuid.c_str(), static_cast<rapidjson::SizeType>(uuid.size()));
                break;
            }
            case ScriptValueKind::Vector2:
            case ScriptValueKind::Vector3:
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Color:
            case ScriptValueKind::Quaternion: {
                const uint32_t count = value.Kind == ScriptValueKind::Vector2 ? 2 : value.Kind == ScriptValueKind::Vector3 ? 3 : 4;
                writer.StartArray();
                for (uint32_t index = 0; index < count; ++index)
                    writer.Double(value.VectorValue[index]);
                writer.EndArray();
                break;
            }
            case ScriptValueKind::Matrix4:
                writer.StartArray();
                for (uint32_t column = 0; column < 4; ++column)
                    for (uint32_t row = 0; row < 4; ++row)
                        writer.Double(value.MatrixValue[column][row]);
                writer.EndArray();
                break;
            case ScriptValueKind::Array:
            case ScriptValueKind::List:
            case ScriptValueKind::Dictionary:
                writer.StartArray();
                for (const ScriptValue& element : value.Elements)
                    WriteValue(writer, element);
                writer.EndArray();
                break;
            case ScriptValueKind::Object:
                writer.StartObject();
                for (const auto& [name, member] : value.Members)
                {
                    writer.Key(name.c_str(), static_cast<rapidjson::SizeType>(name.size()));
                    WriteValue(writer, member);
                }
                writer.EndObject();
                break;
            }
        }

        template <class Writer> void WriteInspectorAttributes(Writer& writer, const ScriptInspectorAttributeSet& attributes)
        {
            if (const ScriptLabelSettings* label = attributes.Get<ScriptLabelSettings>())
            {
                writer.Key("Label");
                writer.String(label->Text.c_str());
            }
            if (const ScriptTooltipSettings* tooltip = attributes.Get<ScriptTooltipSettings>())
            {
                writer.Key("Tooltip");
                writer.String(tooltip->Text.c_str());
            }
            if (const ScriptSearchSettings* settings = attributes.Get<ScriptSearchSettings>())
            {
                writer.Key("Searchable");
                writer.StartObject();
                writer.Key("FilterOptions");
                writer.Uint(static_cast<uint32_t>(settings->FilterOptions));
                writer.Key("FuzzySearch");
                writer.Bool(settings->FuzzySearch);
                writer.Key("Recursive");
                writer.Bool(settings->Recursive);
                writer.EndObject();
            }
            if (const ScriptProgressBarSettings* progressBar = attributes.Get<ScriptProgressBarSettings>())
            {
                writer.Key("ProgressBar");
                writer.StartObject();
                writer.Key("Min");
                writer.Double(progressBar->Min);
                writer.Key("Max");
                writer.Double(progressBar->Max);
                writer.Key("MinGetter");
                writer.String(progressBar->MinGetter.c_str());
                writer.Key("MaxGetter");
                writer.String(progressBar->MaxGetter.c_str());
                writer.Key("R");
                writer.Double(progressBar->Color.r);
                writer.Key("G");
                writer.Double(progressBar->Color.g);
                writer.Key("B");
                writer.Double(progressBar->Color.b);
                writer.Key("Height");
                writer.Uint(progressBar->Height);
                writer.Key("Segmented");
                writer.Bool(progressBar->Segmented);
                writer.Key("DrawValueLabel");
                writer.Bool(progressBar->DrawValueLabel);
                writer.Key("ValueLabelAlignment");
                writer.Uint(static_cast<uint32_t>(progressBar->ValueLabelAlignment));
                writer.Key("ColorGetter");
                writer.String(progressBar->ColorGetter.c_str());
                writer.Key("BackgroundColorGetter");
                writer.String(progressBar->BackgroundColorGetter.c_str());
                writer.Key("CustomValueStringGetter");
                writer.String(progressBar->CustomValueStringGetter.c_str());
                writer.EndObject();
            }
            if (const ScriptPathSettings* settings = attributes.Get<ScriptPathSettings>())
            {
                writer.Key(settings->Kind == ScriptPathKind::File ? "FilePath" : "FolderPath");
                writer.StartObject();
                writer.Key("AbsolutePath");
                writer.Bool(settings->AbsolutePath);
                writer.Key("ParentFolder");
                writer.String(settings->ParentFolder.c_str());
                writer.Key("RequireExistingPath");
                writer.Bool(settings->RequireExistingPath);
                writer.Key("UseBackslashes");
                writer.Bool(settings->UseBackslashes);
                if (settings->Kind == ScriptPathKind::File)
                {
                    writer.Key("Extensions");
                    writer.String(settings->Extensions.c_str());
                    writer.Key("IncludeFileExtension");
                    writer.Bool(settings->IncludeFileExtension);
                }
                writer.EndObject();
            }
            if (const ScriptMultilineSettings* settings = attributes.Get<ScriptMultilineSettings>())
            {
                writer.Key("Multiline");
                writer.StartObject();
                writer.Key("Lines");
                writer.Int(settings->Lines);
                writer.EndObject();
            }
            if (const ScriptColorUsageSettings* settings = attributes.Get<ScriptColorUsageSettings>())
            {
                writer.Key("ColorUsage");
                writer.StartObject();
                writer.Key("ShowAlpha");
                writer.Bool(settings->ShowAlpha);
                writer.Key("Hdr");
                writer.Bool(settings->Hdr);
                writer.EndObject();
            }
            if (const ScriptEnumButtonsSettings* settings = attributes.Get<ScriptEnumButtonsSettings>())
            {
                writer.Key("EnumButtons");
                writer.StartObject();
                writer.Key("IsFlags");
                writer.Bool(settings->IsFlags);
                writer.Key("IsUnsigned");
                writer.Bool(settings->IsUnsigned);
                writer.Key("IncludeObsolete");
                writer.Bool(settings->IncludeObsolete);
                writer.Key("Options");
                writer.StartArray();
                for (const ScriptEnumOption& option : settings->Options)
                {
                    writer.StartObject();
                    writer.Key("Name");
                    writer.String(option.Name.c_str());
                    writer.Key("Value");
                    writer.Uint64(option.Value);
                    writer.EndObject();
                }
                writer.EndArray();
                writer.EndObject();
            }
            if (const ScriptDictionaryDisplaySettings* settings = attributes.Get<ScriptDictionaryDisplaySettings>())
            {
                writer.Key("DictionaryDisplay");
                writer.StartObject();
                writer.Key("Layout");
                writer.Uint(static_cast<uint32_t>(settings->Layout));
                writer.Key("KeyLabel");
                writer.String(settings->KeyLabel.c_str());
                writer.Key("ValueLabel");
                writer.String(settings->ValueLabel.c_str());
                writer.Key("KeyColumnFraction");
                writer.Double(settings->KeyColumnFraction);
                writer.EndObject();
            }
            if (const ScriptConditionalSettings* settings = attributes.Get<ScriptConditionalSettings>())
            {
                writer.Key("Conditions");
                writer.StartArray();
                for (const ScriptConditionRule& rule : settings->Rules)
                {
                    writer.StartObject();
                    writer.Key("Effect");
                    writer.Uint(static_cast<uint32_t>(rule.Effect));
                    writer.Key("Condition");
                    writer.String(rule.Condition.c_str());
                    writer.Key("Animate");
                    writer.Bool(rule.Animate);
                    writer.Key("HasValue");
                    writer.Bool(rule.HasValue);
                    writer.Key("ValueKind");
                    writer.String(KindName(rule.Value.Kind));
                    writer.Key("Value");
                    WriteValue(writer, rule.Value);
                    if (rule.HasResolvedResult)
                    {
                        writer.Key("Result");
                        writer.Bool(rule.ResolvedResult);
                    }
                    writer.EndObject();
                }
                writer.EndArray();
            }
            if (const ScriptOnValueChangedSettings* settings = attributes.Get<ScriptOnValueChangedSettings>())
            {
                writer.Key("OnValueChanged");
                writer.StartArray();
                for (const ScriptValueChangedAction& action : settings->Actions)
                {
                    writer.StartObject();
                    writer.Key("Action");
                    writer.String(action.Action.c_str());
                    writer.Key("MethodId");
                    writer.Uint64(action.MethodId);
                    writer.Key("IncludeChildren");
                    writer.Bool(action.IncludeChildren);
                    writer.Key("InvokeOnInitialize");
                    writer.Bool(action.InvokeOnInitialize);
                    writer.Key("InvokeOnUndoRedo");
                    writer.Bool(action.InvokeOnUndoRedo);
                    writer.Key("PassValue");
                    writer.Bool(action.PassValue);
                    writer.EndObject();
                }
                writer.EndArray();
            }
        }

        template <class Writer> void WriteValueMetadata(Writer& writer, const ScriptValue& value)
        {
            writer.StartObject();
            writer.Key("Kind");
            writer.String(KindName(value.Kind));
            if (value.DeclaredType.IsValid())
            {
                writer.Key("Assembly");
                writer.String(value.DeclaredType.Assembly.c_str());
                writer.Key("Namespace");
                writer.String(value.DeclaredType.Namespace.c_str());
                writer.Key("TypeName");
                writer.String(value.DeclaredType.TypeName.c_str());
            }
            if (value.Kind == ScriptValueKind::Enum)
            {
                writer.Key("EnumUnsigned");
                writer.Bool(value.EnumUnsigned);
            }
            WriteInspectorAttributes(writer, value.Attributes);
            if (!value.Members.empty())
            {
                writer.Key("Members");
                writer.StartObject();
                for (const auto& [name, member] : value.Members)
                {
                    writer.Key(name.c_str(), static_cast<rapidjson::SizeType>(name.size()));
                    WriteValueMetadata(writer, member);
                }
                writer.EndObject();
            }
            if (!value.Elements.empty())
            {
                writer.Key("Elements");
                writer.StartArray();
                for (const ScriptValue& element : value.Elements)
                    WriteValueMetadata(writer, element);
                writer.EndArray();
            }
            writer.EndObject();
        }

        bool TryParseEvent(StringView name, ScriptEventKind& output)
        {
            static const Map<StringView, ScriptEventKind> events = {
                { "Start", ScriptEventKind::Start },
                { "Update", ScriptEventKind::Update },
                { "Destroy", ScriptEventKind::Destroy },
                { "CollisionEnter2D", ScriptEventKind::CollisionEnter2D },
                { "CollisionStay2D", ScriptEventKind::CollisionStay2D },
                { "CollisionExit2D", ScriptEventKind::CollisionExit2D },
                { "TriggerEnter2D", ScriptEventKind::TriggerEnter2D },
                { "TriggerStay2D", ScriptEventKind::TriggerStay2D },
                { "TriggerExit2D", ScriptEventKind::TriggerExit2D },
                { "CollisionEnter3D", ScriptEventKind::CollisionEnter3D },
                { "CollisionStay3D", ScriptEventKind::CollisionStay3D },
                { "CollisionExit3D", ScriptEventKind::CollisionExit3D },
                { "TriggerEnter3D", ScriptEventKind::TriggerEnter3D },
                { "TriggerStay3D", ScriptEventKind::TriggerStay3D },
                { "TriggerExit3D", ScriptEventKind::TriggerExit3D },
                { "FixedUpdate", ScriptEventKind::FixedUpdate },
            };
            const auto event = events.find(name);
            if (event == events.end())
                return false;
            output = event->second;
            return true;
        }
    } // namespace

    ManagedOperationResult ParseManagedCatalogJson(StringView json, ScriptCatalog& catalog, ManagedBackendId backend)
    {
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("ManifestVersion") || !document["ManifestVersion"].IsUint() ||
            document["ManifestVersion"].GetUint() != MANAGED_CATALOG_VERSION || !document.HasMember("Types") || !document["Types"].IsArray())
            return ManagedOperationResult::Failure("managed.catalog.json_invalid", "The managed host returned an invalid script catalog.", backend);
        ScriptCatalog parsed;
        parsed.ManifestVersion = document["ManifestVersion"].GetUint();
        parsed.ManifestHash = Hash(json);
        if (document.HasMember("DictionaryDisplays"))
        {
            if (!document["DictionaryDisplays"].IsArray())
                return ManagedOperationResult::Failure("managed.catalog.json_dictionary_display_invalid",
                                                       "The managed catalog contains an invalid dictionary display collection.", backend);
            for (const rapidjson::Value& ruleValue : document["DictionaryDisplays"].GetArray())
            {
                ScriptDictionaryDisplayRule rule;
                if (!ruleValue.IsObject() || !ruleValue.HasMember("TargetType") || !ReadIdentity(ruleValue["TargetType"], rule.TargetType) ||
                    !ReadDictionaryDisplaySettings(ruleValue, rule.Display))
                    return ManagedOperationResult::Failure("managed.catalog.json_dictionary_display_invalid",
                                                           "The managed catalog contains invalid dictionary display settings.", backend);
                parsed.DictionaryDisplays.push_back(std::move(rule));
            }
        }
        for (const rapidjson::Value& value : document["Types"].GetArray())
        {
            ScriptTypeSchema type;
            if (!value.IsObject() || !value.HasMember("StableId") || !value["StableId"].IsUint64() ||
                !ReadString(value, "Assembly", type.Identity.Assembly) || !ReadString(value, "Namespace", type.Identity.Namespace) ||
                !ReadString(value, "TypeName", type.Identity.TypeName))
                return ManagedOperationResult::Failure("managed.catalog.json_type_invalid", "The managed catalog contains an invalid type.", backend);
            type.StableId = value["StableId"].GetUint64();
            if (value.HasMember("RunInEditor"))
            {
                if (!value["RunInEditor"].IsBool())
                    return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                           "The managed catalog contains an invalid edit-mode flag.", backend);
                if (value["RunInEditor"].GetBool())
                    type.Flags = type.Flags | ScriptTypeFlags::RunInEditor;
            }
            if (value.HasMember("Searchable"))
            {
                ScriptSearchSettings search;
                if (!ReadSearchSettings(value["Searchable"], search))
                    return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                           "The managed catalog contains invalid search settings.", backend);
                type.Attributes.Set(std::move(search));
            }
            if (value.HasMember("BaseType") && !value["BaseType"].IsNull() && !ReadIdentity(value["BaseType"], type.BaseType))
                return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                       "The managed catalog contains an invalid base type identity.", backend);
            if (value.HasMember("Fields") && !value["Fields"].IsArray())
                return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                       "The managed catalog contains an invalid field collection.", backend);
            if (value.HasMember("Fields"))
            {
                for (const rapidjson::Value& fieldValue : value["Fields"].GetArray())
                {
                    ScriptFieldSchema field;
                    String kind;
                    if (!fieldValue.IsObject() || !fieldValue.HasMember("StableId") || !fieldValue["StableId"].IsUint64() ||
                        !ReadString(fieldValue, "Name", field.Name) || !ReadString(fieldValue, "ValueKind", kind))
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid", "The managed catalog contains an invalid field.",
                                                               backend);
                    field.StableId = fieldValue["StableId"].GetUint64();
                    if (!TryParseKind(kind, field.ValueKind))
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                               "The managed catalog contains an unknown field kind.", backend);
                    String elementKind;
                    if (fieldValue.HasMember("ElementKind") && !fieldValue["ElementKind"].IsNull())
                    {
                        if (!ReadString(fieldValue, "ElementKind", elementKind) || !TryParseKind(elementKind, field.ElementKind))
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an unknown element kind.", backend);
                    }
                    String keyKind;
                    if (fieldValue.HasMember("KeyKind") && !fieldValue["KeyKind"].IsNull())
                    {
                        if (!ReadString(fieldValue, "KeyKind", keyKind) || !TryParseKind(keyKind, field.KeyKind))
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an unknown key kind.", backend);
                    }
                    if (fieldValue.HasMember("DeclaredType") && !fieldValue["DeclaredType"].IsNull() &&
                        !ReadIdentity(fieldValue["DeclaredType"], field.DeclaredType))
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                               "The managed catalog contains an invalid declared field type.", backend);
                    bool serializable = true;
                    if (fieldValue.HasMember("IsSerializable"))
                    {
                        if (!fieldValue["IsSerializable"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid serializable flag.", backend);
                        serializable = fieldValue["IsSerializable"].GetBool();
                    }
                    bool inspectable = true;
                    if (fieldValue.HasMember("IsInspectable"))
                    {
                        if (!fieldValue["IsInspectable"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid inspectable flag.", backend);
                        inspectable = fieldValue["IsInspectable"].GetBool();
                    }
                    field.Flags = ScriptSchemaFieldFlags::None;
                    if (serializable)
                        field.Flags = field.Flags | ScriptSchemaFieldFlags::Serializable;
                    if (inspectable)
                        field.Flags = field.Flags | ScriptSchemaFieldFlags::Inspectable;
                    if (fieldValue.HasMember("IsReadOnly"))
                    {
                        if (!fieldValue["IsReadOnly"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid read-only flag.", backend);
                        if (fieldValue["IsReadOnly"].GetBool())
                            field.Flags = field.Flags | ScriptSchemaFieldFlags::ReadOnly;
                    }
                    if (fieldValue.HasMember("IsNullable"))
                    {
                        if (!fieldValue["IsNullable"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid nullable flag.", backend);
                        if (fieldValue["IsNullable"].GetBool())
                            field.Flags = field.Flags | ScriptSchemaFieldFlags::Nullable;
                    }
                    if (!ReadInspectorAttributes(fieldValue, field.Attributes))
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                               "The managed catalog contains invalid inspector attribute settings.", backend);
                    type.Fields.push_back(std::move(field));
                }
            }
            if (value.HasMember("Methods") && !value["Methods"].IsArray())
                return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                       "The managed catalog contains an invalid method collection.", backend);
            if (value.HasMember("Methods"))
            {
                for (const rapidjson::Value& methodValue : value["Methods"].GetArray())
                {
                    ScriptMethodSchema method;
                    String returnKind;
                    if (!methodValue.IsObject() || !methodValue.HasMember("StableId") || !methodValue["StableId"].IsUint64() ||
                        !ReadString(methodValue, "Name", method.Name) || !ReadString(methodValue, "ReturnKind", returnKind) ||
                        !TryParseKind(returnKind, method.ReturnKind) || !methodValue.HasMember("IsStatic") ||
                        !methodValue["IsStatic"].IsBool() || !methodValue.HasMember("Parameters") ||
                        !methodValue["Parameters"].IsArray() || !methodValue.HasMember("Button") || !methodValue["Button"].IsObject())
                        return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                               "The managed catalog contains an invalid button method.", backend);
                    method.StableId = methodValue["StableId"].GetUint64();
                    method.IsStatic = methodValue["IsStatic"].GetBool();
                    if (methodValue.HasMember("DeclaredReturnType") && !methodValue["DeclaredReturnType"].IsNull() &&
                        !ReadIdentity(methodValue["DeclaredReturnType"], method.DeclaredReturnType))
                        return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                               "The managed catalog contains an invalid button return type.", backend);
                    for (const rapidjson::Value& parameterValue : methodValue["Parameters"].GetArray())
                    {
                        ScriptMethodParameterSchema parameter;
                        String kind;
                        if (!parameterValue.IsObject() || !ReadString(parameterValue, "Name", parameter.Name) ||
                            !ReadString(parameterValue, "ValueKind", kind) || !TryParseKind(kind, parameter.ValueKind) ||
                            !parameterValue.HasMember("HasDefaultValue") || !parameterValue["HasDefaultValue"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                                   "The managed catalog contains an invalid button parameter.", backend);
                        parameter.HasDefaultValue = parameterValue["HasDefaultValue"].GetBool();
                        if (parameterValue.HasMember("DeclaredType") && !parameterValue["DeclaredType"].IsNull() &&
                            !ReadIdentity(parameterValue["DeclaredType"], parameter.DeclaredType))
                            return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                                   "The managed catalog contains an invalid button parameter type.", backend);
                        if (parameter.HasDefaultValue)
                        {
                            if (!parameterValue.HasMember("DefaultValue"))
                                return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                                       "The managed catalog omits a button parameter default.", backend);
                            parameter.DefaultValue = ReadValue(parameterValue["DefaultValue"], parameter.ValueKind);
                            parameter.DefaultValue.DeclaredType = parameter.DeclaredType;
                        }
                        method.Parameters.push_back(std::move(parameter));
                    }

                    const rapidjson::Value& buttonValue = methodValue["Button"];
                    ScriptButtonSettings button;
                    const auto height = buttonValue.FindMember("ButtonHeight");
                    const auto alignment = buttonValue.FindMember("ButtonAlignment");
                    const auto style = buttonValue.FindMember("Style");
                    const auto iconAlignment = buttonValue.FindMember("IconAlignment");
                    if (!ReadString(buttonValue, "Name", button.Name) || height == buttonValue.MemberEnd() || !height->value.IsInt() ||
                        alignment == buttonValue.MemberEnd() || !alignment->value.IsNumber() || style == buttonValue.MemberEnd() ||
                        !style->value.IsUint() || style->value.GetUint() > static_cast<uint32_t>(ScriptButtonStyle::FoldoutButton) ||
                        iconAlignment == buttonValue.MemberEnd() || !iconAlignment->value.IsUint() ||
                        iconAlignment->value.GetUint() > static_cast<uint32_t>(ScriptButtonIconAlignment::Right) ||
                        !buttonValue.HasMember("Stretch") || !buttonValue["Stretch"].IsBool() ||
                        !buttonValue.HasMember("DisplayParameters") || !buttonValue["DisplayParameters"].IsBool() ||
                        !buttonValue.HasMember("Expanded") || !buttonValue["Expanded"].IsBool() ||
                        !buttonValue.HasMember("DrawResult") || !buttonValue["DrawResult"].IsBool() ||
                        !buttonValue.HasMember("DirtyOnClick") || !buttonValue["DirtyOnClick"].IsBool() ||
                        !ReadString(buttonValue, "Icon", button.Icon))
                        return ManagedOperationResult::Failure("managed.catalog.json_method_invalid",
                                                               "The managed catalog contains invalid button settings.", backend);
                    button.ButtonHeight = height->value.GetInt();
                    button.ButtonAlignment = static_cast<float>(alignment->value.GetDouble());
                    button.Stretch = buttonValue["Stretch"].GetBool();
                    button.Style = static_cast<ScriptButtonStyle>(style->value.GetUint());
                    button.DisplayParameters = buttonValue["DisplayParameters"].GetBool();
                    button.Expanded = buttonValue["Expanded"].GetBool();
                    button.DrawResult = buttonValue["DrawResult"].GetBool();
                    button.DirtyOnClick = buttonValue["DirtyOnClick"].GetBool();
                    button.IconAlignment = static_cast<ScriptButtonIconAlignment>(iconAlignment->value.GetUint());
                    method.Attributes.Set(std::move(button));
                    type.Methods.push_back(std::move(method));
                }
            }
            if (value.HasMember("Events") && !value["Events"].IsArray())
                return ManagedOperationResult::Failure("managed.catalog.json_event_invalid",
                                                       "The managed catalog contains an invalid event collection.", backend);
            if (value.HasMember("Events"))
            {
                for (const rapidjson::Value& event : value["Events"].GetArray())
                {
                    ScriptEventKind eventKind;
                    if (!event.IsString() || !TryParseEvent(StringView(event.GetString(), event.GetStringLength()), eventKind))
                        return ManagedOperationResult::Failure("managed.catalog.json_event_invalid",
                                                               "The managed catalog contains an unknown script event.", backend);
                    type.Events.push_back(eventKind);
                }
            }
            parsed.Types.push_back(std::move(type));
        }
        ManagedOperationResult validation = ValidateScriptCatalog(parsed, backend);
        if (validation.Succeeded)
            catalog = std::move(parsed);
        return validation;
    }

    ManagedOperationResult ParseManagedStateJson(StringView json, ScriptState& state, ManagedBackendId backend, const ScriptTypeSchema* schema)
    {
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        ScriptState parsed;
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("StateVersion") || !document["StateVersion"].IsUint() ||
            document["StateVersion"].GetUint() != MANAGED_STATE_VERSION || !ReadString(document, "Assembly", parsed.Identity.Assembly) ||
            !ReadString(document, "Namespace", parsed.Identity.Namespace) || !ReadString(document, "TypeName", parsed.Identity.TypeName) ||
            !document.HasMember("Fields") || !document["Fields"].IsObject())
            return ManagedOperationResult::Failure("managed.state.json_invalid", "The managed host returned invalid script state.", backend);
        if (!document.HasMember("Metadata") || !document["Metadata"].IsObject())
            return ManagedOperationResult::Failure("managed.state.json_invalid", "Managed state contains invalid field metadata.", backend);
        const rapidjson::Value& metadata = document["Metadata"];
        parsed.Root = ScriptValue::Object({});
        const auto readMembers = [&](const rapidjson::Value& members, Map<String, ScriptValue>& output) {
            for (auto member = members.MemberBegin(); member != members.MemberEnd(); ++member)
            {
                const String name(member->name.GetString(), member->name.GetStringLength());
                ScriptValueKind expectedKind = ScriptValueKind::Null;
                const ScriptFieldSchema* schemaField = nullptr;
                if (schema != nullptr)
                {
                    const auto field = std::find_if(schema->Fields.begin(), schema->Fields.end(),
                                                    [&](const ScriptFieldSchema& candidate) { return candidate.Name == name; });
                    if (field != schema->Fields.end())
                    {
                        expectedKind = field->ValueKind;
                        schemaField = &*field;
                    }
                }
                const auto encodedMetadata = metadata.FindMember(member->name.GetString());
                if (encodedMetadata == metadata.MemberEnd())
                    return false;
                ScriptValue value = ReadValueWithMetadata(member->value, &encodedMetadata->value, expectedKind);
                if (!value.DeclaredType.IsValid() && schemaField != nullptr)
                    value.DeclaredType = schemaField->DeclaredType;
                if (value.Kind == ScriptValueKind::Enum && schemaField != nullptr)
                {
                    if (const ScriptEnumButtonsSettings* enumButtons = schemaField->Attributes.Get<ScriptEnumButtonsSettings>())
                        value.EnumUnsigned = enumButtons->IsUnsigned;
                }
                output.emplace(name, std::move(value));
            }
            return true;
        };
        try
        {
            if (!readMembers(document["Fields"], parsed.Root.Members))
                return ManagedOperationResult::Failure("managed.state.json_invalid", "Managed state is missing field metadata.", backend);
        }
        catch (const std::exception& exception)
        {
            return ManagedOperationResult::Failure("managed.state.json_invalid", exception.what(), backend);
        }
        parsed.Root.DeclaredType = parsed.Identity;
        state = std::move(parsed);
        return ManagedOperationResult::Success();
    }

    String WriteManagedStateJson(const ScriptState& state)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject();
        writer.Key("StateVersion");
        writer.Uint(MANAGED_STATE_VERSION);
        writer.Key("Assembly");
        writer.String(state.Identity.Assembly.c_str());
        writer.Key("Namespace");
        writer.String(state.Identity.Namespace.c_str());
        writer.Key("TypeName");
        writer.String(state.Identity.TypeName.c_str());
        const Map<String, ScriptValue> empty;
        const Map<String, ScriptValue>& members = state.Root.Kind == ScriptValueKind::Object ? state.Root.Members : empty;
        writer.Key("Metadata");
        writer.StartObject();
        for (const auto& [name, value] : members)
        {
            writer.Key(name.c_str(), static_cast<rapidjson::SizeType>(name.size()));
            WriteValueMetadata(writer, value);
        }
        writer.EndObject();
        writer.Key("Fields");
        WriteValue(writer, ScriptValue::Object(members));
        writer.EndObject();
        return String(buffer.GetString(), buffer.GetSize());
    }

    String WriteManagedArgumentsJson(const Vector<ScriptValue>& arguments)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartArray();
        for (const ScriptValue& argument : arguments)
            WriteValue(writer, argument);
        writer.EndArray();
        return String(buffer.GetString(), buffer.GetSize());
    }

    ScriptInvocationResult ParseManagedInvocationResultJson(StringView json, ManagedBackendId backend)
    {
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("HasResult") ||
            !document["HasResult"].IsBool() || !document.HasMember("ResultKind") || !document["ResultKind"].IsString() ||
            !document.HasMember("Result"))
            return { ManagedOperationResult::Failure("managed.button.result_invalid",
                                                     "The managed button returned an invalid result payload.", backend),
                     false,
                     {} };
        ScriptInvocationResult result;
        result.Result = ManagedOperationResult::Success();
        result.HasReturnValue = document["HasResult"].GetBool();
        if (!result.HasReturnValue)
            return result;
        ScriptValueKind kind;
        if (!TryParseKind(StringView(document["ResultKind"].GetString(), document["ResultKind"].GetStringLength()), kind))
            return { ManagedOperationResult::Failure("managed.button.result_kind_invalid",
                                                     "The managed button returned an unknown result kind.", backend),
                     false,
                     {} };
        result.ReturnValue = ReadValue(document["Result"], kind);
        return result;
    }

    Vector<ManagedDiagnostic> ParseManagedDiagnosticsJson(StringView json, ManagedBackendId backend)
    {
        Vector<ManagedDiagnostic> diagnostics;
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsArray())
            return diagnostics;
        for (const rapidjson::Value& value : document.GetArray())
        {
            if (!value.IsObject())
                continue;
            ManagedDiagnostic diagnostic;
            diagnostic.Backend = backend;
            String severity;
            if (ReadString(value, "Severity", severity))
            {
                diagnostic.Severity = severity == "Info"      ? ManagedDiagnosticSeverity::Info
                                      : severity == "Warning" ? ManagedDiagnosticSeverity::Warning
                                                              : ManagedDiagnosticSeverity::Error;
            }
            ReadString(value, "Code", diagnostic.Code);
            ReadString(value, "Message", diagnostic.Message);
            ReadString(value, "Stack", diagnostic.ManagedStack);
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }
} // namespace Crowny
