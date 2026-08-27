#include "cwpch.h"

#include "Crowny/Scripting/Managed/ManagedTypes.h"

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
        if (Kind != other.Kind || DeclaredType != other.DeclaredType)
            return false;

        switch (Kind)
        {
        case ScriptValueKind::Null: return true;
        case ScriptValueKind::Boolean: return BooleanValue == other.BooleanValue;
        case ScriptValueKind::SignedInteger: return SignedValue == other.SignedValue;
        case ScriptValueKind::UnsignedInteger: return UnsignedValue == other.UnsignedValue;
        case ScriptValueKind::Float: return FloatingValue == other.FloatingValue;
        case ScriptValueKind::String: return StringValue == other.StringValue;
        case ScriptValueKind::Enum: return SignedValue == other.SignedValue;
        case ScriptValueKind::Vector2:
        case ScriptValueKind::Vector3:
        case ScriptValueKind::Vector4:
        case ScriptValueKind::Quaternion: return VectorValue == other.VectorValue;
        case ScriptValueKind::Matrix4: return MatricesEqual(MatrixValue, other.MatrixValue);
        case ScriptValueKind::Entity:
        case ScriptValueKind::Asset:
        case ScriptValueKind::Uuid: return ReferenceValue == other.ReferenceValue;
        case ScriptValueKind::Array:
        case ScriptValueKind::List:
        case ScriptValueKind::Dictionary: return Elements == other.Elements;
        case ScriptValueKind::Object: return Members == other.Members;
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
        const auto type = std::find_if(Types.begin(), Types.end(), [&](const ScriptTypeSchema& candidate) {
            return candidate.Identity == identity ||
                   std::find(candidate.FormerIdentities.begin(), candidate.FormerIdentities.end(), identity) !=
                     candidate.FormerIdentities.end();
        });
        return type != Types.end() ? &*type : nullptr;
    }

    ManagedOperationResult ManagedOperationResult::Success() { return {}; }

    ManagedOperationResult ManagedOperationResult::Failure(String code, String message, ManagedBackendId backend)
    {
        ManagedOperationResult result;
        result.Succeeded = false;
        result.Diagnostics.push_back(
          { ManagedDiagnosticSeverity::Error, std::move(code), std::move(message), {}, backend, {}, {} });
        return result;
    }

    bool ManagedOperationResult::HasDiagnosticCode(StringView code) const
    {
        return std::any_of(Diagnostics.begin(), Diagnostics.end(),
                           [&](const ManagedDiagnostic& diagnostic) { return diagnostic.Code == code; });
    }

    const char* ToString(ManagedBackendId backend)
    {
        switch (backend)
        {
        case ManagedBackendId::Mono: return "Mono";
        case ManagedBackendId::CoreCLR: return "CoreCLR";
        case ManagedBackendId::DotNetWasm: return "DotNetWasm";
        case ManagedBackendId::NativeAOT: return "NativeAOT";
        case ManagedBackendId::GeneratedMetadata: return "GeneratedMetadata";
        }
        return "Unknown";
    }

    const char* ToString(ManagedExecutionMode mode)
    {
        switch (mode)
        {
        case ManagedExecutionMode::Interpreter: return "Interpreter";
        case ManagedExecutionMode::Jit: return "Jit";
        case ManagedExecutionMode::ReadyToRun: return "ReadyToRun";
        case ManagedExecutionMode::Aot: return "Aot";
        }
        return "Unknown";
    }

    ManagedOperationResult ValidateScriptCatalog(const ScriptCatalog& catalog, ManagedBackendId backend)
    {
        if (catalog.ManifestVersion == 0 || catalog.ManifestHash == 0)
            return ManagedOperationResult::Failure("managed.catalog.manifest_invalid",
                                                   "The script catalog has no manifest version or content hash.", backend);

        Set<uint64_t> typeIds;
        Set<String> identities;
        for (const ScriptTypeSchema& type : catalog.Types)
        {
            if (!type.Identity.IsValid() || type.StableId == 0 || !typeIds.insert(type.StableId).second)
                return ManagedOperationResult::Failure("managed.catalog.type_invalid",
                                                       "The script catalog contains an invalid or duplicate type.", backend);

            Vector<ScriptTypeIdentity> allIdentities = type.FormerIdentities;
            allIdentities.push_back(type.Identity);
            for (const ScriptTypeIdentity& identity : allIdentities)
            {
                const String key = identity.Assembly + ":" + identity.GetFullName();
                if (!identity.IsValid() || !identities.insert(key).second)
                    return ManagedOperationResult::Failure("managed.catalog.type_identity_collision",
                                                           "The script catalog contains an invalid or colliding type identity.", backend);
            }

            Set<uint64_t> fieldIds;
            Set<String> fieldNames;
            for (const ScriptFieldSchema& field : type.Fields)
            {
                if (field.StableId == 0 || field.Name.empty() || !fieldIds.insert(field.StableId).second ||
                    !fieldNames.insert(field.Name).second)
                    return ManagedOperationResult::Failure("managed.catalog.field_invalid",
                                                           "The script catalog contains an invalid or duplicate field.", backend);
                for (const String& formerName : field.FormerNames)
                {
                    if (formerName.empty() || !fieldNames.insert(formerName).second)
                        return ManagedOperationResult::Failure("managed.catalog.field_name_collision",
                                                               "The script catalog contains a colliding former field name.", backend);
                }
            }
        }
        return ManagedOperationResult::Success();
    }

    ScriptStateResult MigrateScriptState(const ScriptState& state, const ScriptTypeSchema& target, ManagedBackendId backend)
    {
        const bool compatibleIdentity = !state.Identity.IsValid() || state.Identity == target.Identity ||
                                        std::find(target.FormerIdentities.begin(), target.FormerIdentities.end(), state.Identity) !=
                                          target.FormerIdentities.end();
        if (!compatibleIdentity)
            return { ManagedOperationResult::Failure("managed.script.state_identity_mismatch",
                                                     "Script state belongs to an incompatible script type.", backend),
                     {} };
        if (state.Root.Kind != ScriptValueKind::Null && state.Root.Kind != ScriptValueKind::Object)
            return { ManagedOperationResult::Failure("managed.script.state_root_invalid",
                                                     "Script state must be represented by an object value.", backend),
                     {} };

        ScriptState migrated;
        migrated.Identity = target.Identity;
        migrated.Root = ScriptValue::Object({}, target.Identity);
        migrated.OrphanedMembers = state.OrphanedMembers;
        Map<String, ScriptValue> remaining = state.Root.Members;
        for (const ScriptFieldSchema& field : target.Fields)
        {
            if ((field.Flags & ScriptSchemaFieldFlags::Serializable) == ScriptSchemaFieldFlags::None)
                continue;
            auto value = remaining.find(field.Name);
            if (value == remaining.end())
            {
                for (const String& formerName : field.FormerNames)
                {
                    value = remaining.find(formerName);
                    if (value != remaining.end())
                        break;
                }
            }
            if (value == remaining.end())
                continue;
            if (value->second.Kind == field.ValueKind || value->second.Kind == ScriptValueKind::Null)
                migrated.Root.Members[field.Name] = std::move(value->second);
            else
                migrated.OrphanedMembers[field.Name] = std::move(value->second);
            remaining.erase(value);
        }
        for (auto& [name, value] : remaining)
            migrated.OrphanedMembers[name] = std::move(value);
        return { ManagedOperationResult::Success(), std::move(migrated) };
    }
} // namespace Crowny
