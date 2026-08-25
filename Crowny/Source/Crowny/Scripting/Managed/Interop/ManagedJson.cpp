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
            return value.IsObject() && ReadString(value, "Assembly", output.Assembly) &&
                   ReadString(value, "Namespace", output.Namespace) && ReadString(value, "TypeName", output.TypeName) && output.IsValid();
        }

        bool TryParseKind(StringView value, ScriptValueKind& output)
        {
            static const Map<StringView, ScriptValueKind> kinds = {
                { "Null", ScriptValueKind::Null },           { "Boolean", ScriptValueKind::Boolean },
                { "SignedInteger", ScriptValueKind::SignedInteger }, { "UnsignedInteger", ScriptValueKind::UnsignedInteger },
                { "Float", ScriptValueKind::Float },         { "String", ScriptValueKind::String },
                { "Enum", ScriptValueKind::Enum },           { "Vector2", ScriptValueKind::Vector2 },
                { "Vector3", ScriptValueKind::Vector3 },     { "Vector4", ScriptValueKind::Vector4 },
                { "Quaternion", ScriptValueKind::Quaternion }, { "Matrix4", ScriptValueKind::Matrix4 },
                { "Entity", ScriptValueKind::Entity },       { "Asset", ScriptValueKind::Asset },
                { "Array", ScriptValueKind::Array },         { "List", ScriptValueKind::List },
                { "Dictionary", ScriptValueKind::Dictionary }, { "Object", ScriptValueKind::Object },
                { "Uuid", ScriptValueKind::Uuid },
            };
            const auto kind = kinds.find(value);
            if (kind == kinds.end())
                return false;
            output = kind->second;
            return true;
        }

        ScriptValue ReadValue(const rapidjson::Value& value, ScriptValueKind expectedKind = ScriptValueKind::Null)
        {
            if (value.IsNull())
                return ScriptValue::Null();
            if (expectedKind == ScriptValueKind::Boolean && value.IsBool())
                return ScriptValue::Boolean(value.GetBool());
            if ((expectedKind == ScriptValueKind::SignedInteger || expectedKind == ScriptValueKind::Enum) && value.IsInt64())
            {
                ScriptValue result = ScriptValue::Signed(value.GetInt64());
                result.Kind = expectedKind;
                return result;
            }
            if (expectedKind == ScriptValueKind::UnsignedInteger && value.IsUint64())
                return ScriptValue::Unsigned(value.GetUint64());
            if (expectedKind == ScriptValueKind::Float && value.IsNumber())
                return ScriptValue::Float(value.GetDouble());
            if (expectedKind == ScriptValueKind::String && value.IsString())
                return ScriptValue::Text(String(value.GetString(), value.GetStringLength()));
            if ((expectedKind == ScriptValueKind::Entity || expectedKind == ScriptValueKind::Asset ||
                 expectedKind == ScriptValueKind::Uuid) &&
                value.IsString())
            {
                ScriptValue result;
                result.Kind = expectedKind;
                result.ReferenceValue = UUID(String(value.GetString(), value.GetStringLength()));
                return result;
            }
            if ((expectedKind == ScriptValueKind::Vector2 || expectedKind == ScriptValueKind::Vector3 ||
                 expectedKind == ScriptValueKind::Vector4 || expectedKind == ScriptValueKind::Quaternion) &&
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
            if ((expectedKind == ScriptValueKind::Array || expectedKind == ScriptValueKind::List ||
                 expectedKind == ScriptValueKind::Dictionary) &&
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

        template <class Writer> void WriteValue(Writer& writer, const ScriptValue& value)
        {
            switch (value.Kind)
            {
            case ScriptValueKind::Null: writer.Null(); break;
            case ScriptValueKind::Boolean: writer.Bool(value.BooleanValue); break;
            case ScriptValueKind::SignedInteger:
            case ScriptValueKind::Enum: writer.Int64(value.SignedValue); break;
            case ScriptValueKind::UnsignedInteger: writer.Uint64(value.UnsignedValue); break;
            case ScriptValueKind::Float: writer.Double(value.FloatingValue); break;
            case ScriptValueKind::String: writer.String(value.StringValue.c_str(), static_cast<rapidjson::SizeType>(value.StringValue.size())); break;
            case ScriptValueKind::Entity:
            case ScriptValueKind::Asset:
            case ScriptValueKind::Uuid: {
                const String uuid = value.ReferenceValue.ToString();
                writer.String(uuid.c_str(), static_cast<rapidjson::SizeType>(uuid.size()));
                break;
            }
            case ScriptValueKind::Vector2:
            case ScriptValueKind::Vector3:
            case ScriptValueKind::Vector4:
            case ScriptValueKind::Quaternion:
                writer.StartArray();
                for (uint32_t index = 0; index < 4; ++index)
                    writer.Double(value.VectorValue[index]);
                writer.EndArray();
                break;
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

        bool TryParseEvent(StringView name, ScriptEventKind& output)
        {
            static const Map<StringView, ScriptEventKind> events = {
                { "Start", ScriptEventKind::Start }, { "Update", ScriptEventKind::Update }, { "Destroy", ScriptEventKind::Destroy },
                { "CollisionEnter2D", ScriptEventKind::CollisionEnter2D }, { "CollisionStay2D", ScriptEventKind::CollisionStay2D },
                { "CollisionExit2D", ScriptEventKind::CollisionExit2D }, { "TriggerEnter2D", ScriptEventKind::TriggerEnter2D },
                { "TriggerStay2D", ScriptEventKind::TriggerStay2D }, { "TriggerExit2D", ScriptEventKind::TriggerExit2D },
                { "CollisionEnter3D", ScriptEventKind::CollisionEnter3D }, { "CollisionStay3D", ScriptEventKind::CollisionStay3D },
                { "CollisionExit3D", ScriptEventKind::CollisionExit3D }, { "TriggerEnter3D", ScriptEventKind::TriggerEnter3D },
                { "TriggerStay3D", ScriptEventKind::TriggerStay3D }, { "TriggerExit3D", ScriptEventKind::TriggerExit3D },
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
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("ManifestVersion") ||
            !document["ManifestVersion"].IsUint() || document["ManifestVersion"].GetUint() != 1 || !document.HasMember("Types") ||
            !document["Types"].IsArray())
            return ManagedOperationResult::Failure("managed.catalog.json_invalid", "The managed host returned an invalid script catalog.", backend);
        ScriptCatalog parsed;
        parsed.ManifestVersion = document["ManifestVersion"].GetUint();
        parsed.ManifestHash = Hash(json);
        for (const rapidjson::Value& value : document["Types"].GetArray())
        {
            ScriptTypeSchema type;
            if (!value.IsObject() || !value.HasMember("StableId") || !value["StableId"].IsUint64() ||
                !ReadString(value, "Assembly", type.Identity.Assembly) || !ReadString(value, "Namespace", type.Identity.Namespace) ||
                !ReadString(value, "TypeName", type.Identity.TypeName))
                return ManagedOperationResult::Failure("managed.catalog.json_type_invalid", "The managed catalog contains an invalid type.", backend);
            type.StableId = value["StableId"].GetUint64();
            if (value.HasMember("FormerIdentities"))
            {
                if (!value["FormerIdentities"].IsArray())
                    return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                           "The managed catalog contains invalid former type identities.", backend);
                for (const rapidjson::Value& formerValue : value["FormerIdentities"].GetArray())
                {
                    ScriptTypeIdentity former;
                    if (!ReadIdentity(formerValue, former))
                        return ManagedOperationResult::Failure("managed.catalog.json_type_invalid",
                                                               "The managed catalog contains an invalid former type identity.", backend);
                    type.FormerIdentities.push_back(std::move(former));
                }
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
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid", "The managed catalog contains an invalid field.", backend);
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
                    field.Flags = ScriptFieldFlags::Serializable | ScriptFieldFlags::Inspectable;
                    if (fieldValue.HasMember("IsReadOnly"))
                    {
                        if (!fieldValue["IsReadOnly"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid read-only flag.", backend);
                        if (fieldValue["IsReadOnly"].GetBool())
                            field.Flags = field.Flags | ScriptFieldFlags::ReadOnly;
                    }
                    if (fieldValue.HasMember("IsNullable"))
                    {
                        if (!fieldValue["IsNullable"].IsBool())
                            return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                   "The managed catalog contains an invalid nullable flag.", backend);
                        if (fieldValue["IsNullable"].GetBool())
                            field.Flags = field.Flags | ScriptFieldFlags::Nullable;
                    }
                    if (fieldValue.HasMember("FormerNames") && !fieldValue["FormerNames"].IsArray())
                        return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                               "The managed catalog contains an invalid former-name collection.", backend);
                    if (fieldValue.HasMember("FormerNames"))
                    {
                        for (const rapidjson::Value& formerName : fieldValue["FormerNames"].GetArray())
                        {
                            if (!formerName.IsString())
                                return ManagedOperationResult::Failure("managed.catalog.json_field_invalid",
                                                                       "The managed catalog contains an invalid former field name.", backend);
                            field.FormerNames.emplace_back(formerName.GetString(), formerName.GetStringLength());
                        }
                    }
                    type.Fields.push_back(std::move(field));
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

    ManagedOperationResult ParseManagedStateJson(StringView json, ScriptState& state, ManagedBackendId backend,
                                                 const ScriptTypeSchema* schema)
    {
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        ScriptState parsed;
        if (document.HasParseError() || !document.IsObject() || !ReadString(document, "Assembly", parsed.Identity.Assembly) ||
            !ReadString(document, "Namespace", parsed.Identity.Namespace) || !ReadString(document, "TypeName", parsed.Identity.TypeName) ||
            !document.HasMember("Fields") || !document["Fields"].IsObject())
            return ManagedOperationResult::Failure("managed.state.json_invalid", "The managed host returned invalid script state.", backend);
        parsed.Root = ScriptValue::Object({});
        const rapidjson::Value& fields = document["Fields"];
        for (auto member = fields.MemberBegin(); member != fields.MemberEnd(); ++member)
        {
            const String name(member->name.GetString(), member->name.GetStringLength());
            ScriptValueKind expectedKind = ScriptValueKind::Null;
            if (schema != nullptr)
            {
                const auto field = std::find_if(schema->Fields.begin(), schema->Fields.end(), [&](const ScriptFieldSchema& candidate) {
                    return candidate.Name == name ||
                           std::find(candidate.FormerNames.begin(), candidate.FormerNames.end(), name) != candidate.FormerNames.end();
                });
                if (field != schema->Fields.end())
                    expectedKind = field->ValueKind;
            }
            parsed.Root.Members.emplace(name, ReadValue(member->value, expectedKind));
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
        writer.Key("Assembly"); writer.String(state.Identity.Assembly.c_str());
        writer.Key("Namespace"); writer.String(state.Identity.Namespace.c_str());
        writer.Key("TypeName"); writer.String(state.Identity.TypeName.c_str());
        writer.Key("Fields");
        ScriptValue merged = state.Root;
        if (merged.Kind != ScriptValueKind::Object)
            merged = ScriptValue::Object({});
        for (const auto& [name, value] : state.OrphanedMembers)
            if (merged.Members.find(name) == merged.Members.end())
                merged.Members.emplace(name, value);
        WriteValue(writer, merged);
        writer.EndObject();
        return String(buffer.GetString(), buffer.GetSize());
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
                diagnostic.Severity = severity == "Info" ? ManagedDiagnosticSeverity::Info
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
