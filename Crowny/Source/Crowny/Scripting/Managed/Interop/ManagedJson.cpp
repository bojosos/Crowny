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

            if (value.IsObject() && metadata->HasMember("Members"))
            {
                const rapidjson::Value& members = (*metadata)["Members"];
                if (!members.IsObject())
                    throw std::runtime_error("Managed state contains invalid object metadata");
                result.Members.clear();
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                {
                    const auto memberMetadata = members.FindMember(member->name.GetString());
                    result.Members.emplace(String(member->name.GetString(), member->name.GetStringLength()),
                                           ReadValueWithMetadata(member->value,
                                                                 memberMetadata != members.MemberEnd() ? &memberMetadata->value : nullptr));
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
            case ScriptValueKind::Enum:
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
                    const auto field = std::find_if(schema->Fields.begin(), schema->Fields.end(), [&](const ScriptFieldSchema& candidate) {
                        return candidate.Name == name;
                    });
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
