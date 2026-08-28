#include "cwpch.h"

#include "Crowny/Scripting/Backends/Mono/MonoBackend.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/PhysicsCollision.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/ScriptBindings.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Scripting/Managed/LegacyScriptState.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MONO_DEBUG_PORT = 17615;
        cw_managed_host_api* g_MonoHostApi = nullptr;

        void* CW_MANAGED_CALL GetMonoHostApi() { return g_MonoHostApi; }

        const ManagedProgramArtifact* FindArtifact(const ManagedProgramDefinition& program, ManagedProgramArtifactKind kind)
        {
            const auto artifact = std::find_if(program.Artifacts.begin(), program.Artifacts.end(),
                                               [kind](const ManagedProgramArtifact& value) { return value.Kind == kind; });
            return artifact == program.Artifacts.end() ? nullptr : &*artifact;
        }

        uint64_t StableHash(StringView value)
        {
            constexpr uint64_t OFFSET = 14695981039346656037ull;
            constexpr uint64_t PRIME = 1099511628211ull;
            uint64_t hash = OFFSET;
            for (const uint8_t byte : value)
                hash = (hash ^ byte) * PRIME;
            return hash == 0 ? 1 : hash;
        }

        ScriptTypeIdentity GetTypeIdentity(const Ref<SerializableTypeInfo>& typeInfo)
        {
            if (typeInfo == nullptr)
                return {};
            if (typeInfo->GetType() == SerializableType::Enum)
            {
                const auto info = StaticRefCast<SerializableTypeInfoEnum>(typeInfo);
                return { info->m_AssemblyName, info->m_TypeNamespace, info->m_TypeName };
            }
            if (typeInfo->GetType() == SerializableType::Object)
            {
                const auto info = StaticRefCast<SerializableTypeInfoObject>(typeInfo);
                return { info->m_AssemblyName, info->m_TypeNamespace, info->m_TypeName };
            }
            return {};
        }

        ScriptValueKind GetPrimitiveKind(ScriptPrimitiveType type)
        {
            switch (type)
            {
            case ScriptPrimitiveType::Bool: return ScriptValueKind::Boolean;
            case ScriptPrimitiveType::Char:
            case ScriptPrimitiveType::String: return ScriptValueKind::String;
            case ScriptPrimitiveType::I8:
            case ScriptPrimitiveType::I16:
            case ScriptPrimitiveType::I32:
            case ScriptPrimitiveType::I64: return ScriptValueKind::SignedInteger;
            case ScriptPrimitiveType::U8:
            case ScriptPrimitiveType::U16:
            case ScriptPrimitiveType::U32:
            case ScriptPrimitiveType::U64: return ScriptValueKind::UnsignedInteger;
            case ScriptPrimitiveType::Float:
            case ScriptPrimitiveType::Double: return ScriptValueKind::Float;
            case ScriptPrimitiveType::Vector2: return ScriptValueKind::Vector2;
            case ScriptPrimitiveType::Vector3: return ScriptValueKind::Vector3;
            case ScriptPrimitiveType::Vector4:
            case ScriptPrimitiveType::Color: return ScriptValueKind::Vector4;
            case ScriptPrimitiveType::Matrix4: return ScriptValueKind::Matrix4;
            case ScriptPrimitiveType::Count: return ScriptValueKind::Null;
            }
            return ScriptValueKind::Null;
        }

        ScriptValueKind GetValueKind(const Ref<SerializableTypeInfo>& typeInfo)
        {
            if (typeInfo == nullptr)
                return ScriptValueKind::Null;
            switch (typeInfo->GetType())
            {
            case SerializableType::Primitive:
                return GetPrimitiveKind(StaticRefCast<SerializableTypeInfoPrimitive>(typeInfo)->m_Type);
            case SerializableType::Enum: return ScriptValueKind::Enum;
            case SerializableType::Entity: return ScriptValueKind::Entity;
            case SerializableType::Asset: return ScriptValueKind::Asset;
            case SerializableType::Object: return ScriptValueKind::Object;
            case SerializableType::Array: return ScriptValueKind::Array;
            case SerializableType::List: return ScriptValueKind::List;
            case SerializableType::Dictionary: return ScriptValueKind::Dictionary;
            }
            return ScriptValueKind::Null;
        }

        void AppendFields(const Ref<SerializableObjectInfo>& objectInfo, Vector<ScriptFieldSchema>& fields)
        {
            if (objectInfo == nullptr)
                return;
            AppendFields(objectInfo->m_BaseClass, fields);
            Vector<Ref<SerializableMemberInfo>> members;
            members.reserve(objectInfo->m_Fields.size());
            for (const auto& [id, member] : objectInfo->m_Fields)
                members.push_back(member);
            std::sort(members.begin(), members.end(), [](const auto& left, const auto& right) {
                return std::tie(left->m_ParentTypeId, left->m_FieldId, left->m_Name) <
                       std::tie(right->m_ParentTypeId, right->m_FieldId, right->m_Name);
            });
            for (const Ref<SerializableMemberInfo>& member : members)
            {
                if (!member->IsSerializable())
                    continue;
                const ScriptValueKind valueKind = GetValueKind(member->m_TypeInfo);
                if (valueKind == ScriptValueKind::Null || valueKind == ScriptValueKind::Dictionary)
                    continue;
                ScriptFieldSchema field;
                field.StableId = member->m_FieldId != 0 ? member->m_FieldId : StableHash(member->m_Name);
                field.Name = member->m_Name;
                field.ValueKind = valueKind;
                field.Flags = MonoBackendDetail::GetSchemaFieldFlags(member);
                field.DeclaredType = GetTypeIdentity(member->m_TypeInfo);
                if (member->m_TypeInfo->GetType() == SerializableType::Array)
                    field.ElementKind = GetValueKind(StaticRefCast<SerializableTypeInfoArray>(member->m_TypeInfo)->m_ElementType);
                else if (member->m_TypeInfo->GetType() == SerializableType::List)
                    field.ElementKind = GetValueKind(StaticRefCast<SerializableTypeInfoList>(member->m_TypeInfo)->m_ElementType);
                fields.push_back(std::move(field));
            }
        }

        bool HasMethod(MonoClass* type, StringView name, uint32_t parameterCount)
        {
            for (MonoClass* current = type; current != nullptr; current = current->GetBaseClass())
            {
                if (current->GetMethod(name, parameterCount) != nullptr)
                    return true;
                if (current == ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour)
                    break;
            }
            return false;
        }

        Vector<ScriptEventKind> GetEvents(MonoClass* type)
        {
            struct EventMethod
            {
                ScriptEventKind Kind;
                const char* Name;
                uint32_t Parameters;
            };
            static constexpr EventMethod METHODS[] = {
                { ScriptEventKind::Start, "Start", 0 },
                { ScriptEventKind::Update, "Update", 0 },
                { ScriptEventKind::Destroy, "Destroy", 0 },
                { ScriptEventKind::CollisionEnter2D, "OnCollisionEnter2D", 1 },
                { ScriptEventKind::CollisionStay2D, "OnCollisionStay2D", 1 },
                { ScriptEventKind::CollisionExit2D, "OnCollisionExit2D", 1 },
                { ScriptEventKind::TriggerEnter2D, "OnTriggerEnter2D", 1 },
                { ScriptEventKind::TriggerStay2D, "OnTriggerStay2D", 1 },
                { ScriptEventKind::TriggerExit2D, "OnTriggerExit2D", 1 },
                { ScriptEventKind::CollisionEnter3D, "OnCollisionEnter3D", 1 },
                { ScriptEventKind::CollisionStay3D, "OnCollisionStay3D", 1 },
                { ScriptEventKind::CollisionExit3D, "OnCollisionExit3D", 1 },
                { ScriptEventKind::TriggerEnter3D, "OnTriggerEnter3D", 1 },
                { ScriptEventKind::TriggerStay3D, "OnTriggerStay3D", 1 },
                { ScriptEventKind::TriggerExit3D, "OnTriggerExit3D", 1 },
            };
            Vector<ScriptEventKind> events;
            for (const EventMethod& method : METHODS)
            {
                if (HasMethod(type, method.Name, method.Parameters))
                    events.push_back(method.Kind);
            }
            return events;
        }

        ScriptCatalog BuildCatalog()
        {
            ScriptCatalog catalog;
            catalog.ManifestVersion = 1;
            for (const auto& [name, type] : ScriptInfoManager::Get().GetEntityBehaviours())
            {
                if (type == nullptr || type == ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour)
                    continue;
                MonoAssembly* assembly = MonoManager::Get().FindAssembly(type->GetInternalPtr());
                if (assembly == nullptr)
                    continue;
                Ref<SerializableObjectInfo> objectInfo;
                if (!ScriptInfoManager::Get().GetSerializableObjectInfo(assembly->GetName(), type->GetNamespace(), type->GetName(), objectInfo))
                    continue;
                ScriptTypeSchema schema;
                schema.Identity = { assembly->GetName(), objectInfo->m_TypeInfo->m_TypeNamespace, objectInfo->m_TypeInfo->m_TypeName };
                schema.StableId = objectInfo->m_TypeInfo->m_TypeId != 0 ? objectInfo->m_TypeInfo->m_TypeId
                                                                        : StableHash(schema.Identity.Assembly + ":" + schema.Identity.GetFullName());
                if (objectInfo->m_BaseClass != nullptr && objectInfo->m_BaseClass->m_TypeInfo != nullptr)
                {
                    const auto& base = objectInfo->m_BaseClass->m_TypeInfo;
                    schema.BaseType = { base->m_AssemblyName, base->m_TypeNamespace, base->m_TypeName };
                }
                AppendFields(objectInfo, schema.Fields);
                schema.Events = GetEvents(type);
                MonoClass* runInEditor = ScriptInfoManager::Get().GetBuiltinClasses().RunInEditorAttribute;
                if (runInEditor != nullptr && type->HasAttribute(runInEditor))
                    schema.Flags = schema.Flags | ScriptTypeFlags::RunInEditor;
                catalog.Types.push_back(std::move(schema));
            }
            std::sort(catalog.Types.begin(), catalog.Types.end(), [](const ScriptTypeSchema& left, const ScriptTypeSchema& right) {
                return std::tie(left.Identity.Assembly, left.Identity.Namespace, left.Identity.TypeName) <
                       std::tie(right.Identity.Assembly, right.Identity.Namespace, right.Identity.TypeName);
            });
            uint64_t hash = 14695981039346656037ull;
            for (const ScriptTypeSchema& type : catalog.Types)
            {
                hash ^= StableHash(type.Identity.Assembly + ":" + type.Identity.GetFullName());
                hash *= 1099511628211ull;
                for (const ScriptFieldSchema& field : type.Fields)
                {
                    hash ^= field.StableId;
                    hash *= 1099511628211ull;
                }
            }
            catalog.ManifestHash = hash;
            return catalog;
        }

        ScriptValue ReadLegacyValue(const Ref<SerializableFieldData>& data, const Ref<SerializableTypeInfo>& typeInfo);

        ScriptValue ReadLegacyObject(const Ref<SerializableObject>& object, const Ref<SerializableObjectInfo>& objectInfo)
        {
            if (object == nullptr || objectInfo == nullptr)
                return ScriptValue::Null();
            Map<String, ScriptValue> members;
            Vector<Ref<SerializableObjectInfo>> hierarchy;
            for (Ref<SerializableObjectInfo> current = objectInfo; current != nullptr; current = current->m_BaseClass)
                hierarchy.push_back(current);
            std::reverse(hierarchy.begin(), hierarchy.end());
            for (const Ref<SerializableObjectInfo>& type : hierarchy)
            {
                for (const auto& [id, member] : type->m_Fields)
                {
                    if (!member->IsSerializable())
                        continue;
                    Ref<SerializableFieldData> value = object->GetFieldData(member);
                    if (value != nullptr)
                        members[member->m_Name] = ReadLegacyValue(value, member->m_TypeInfo);
                }
            }
            ScriptTypeIdentity identity{ objectInfo->m_TypeInfo->m_AssemblyName, objectInfo->m_TypeInfo->m_TypeNamespace,
                                         objectInfo->m_TypeInfo->m_TypeName };
            return ScriptValue::Object(std::move(members), std::move(identity));
        }

        ScriptValue ReadLegacyPrimitive(const Ref<SerializableFieldData>& data, ScriptPrimitiveType type)
        {
            switch (type)
            {
            case ScriptPrimitiveType::Bool: return ScriptValue::Boolean(StaticRefCast<SerializableFieldBool>(data)->Value);
            case ScriptPrimitiveType::Char:
                return ScriptValue::Text(String(1, StaticRefCast<SerializableFieldChar>(data)->Value));
            case ScriptPrimitiveType::I8: return ScriptValue::Signed(StaticRefCast<SerializableFieldI8>(data)->Value);
            case ScriptPrimitiveType::U8: return ScriptValue::Unsigned(StaticRefCast<SerializableFieldU8>(data)->Value);
            case ScriptPrimitiveType::I16: return ScriptValue::Signed(StaticRefCast<SerializableFieldI16>(data)->Value);
            case ScriptPrimitiveType::U16: return ScriptValue::Unsigned(StaticRefCast<SerializableFieldU16>(data)->Value);
            case ScriptPrimitiveType::I32: return ScriptValue::Signed(StaticRefCast<SerializableFieldI32>(data)->Value);
            case ScriptPrimitiveType::U32: return ScriptValue::Unsigned(StaticRefCast<SerializableFieldU32>(data)->Value);
            case ScriptPrimitiveType::I64: return ScriptValue::Signed(StaticRefCast<SerializableFieldI64>(data)->Value);
            case ScriptPrimitiveType::U64: return ScriptValue::Unsigned(StaticRefCast<SerializableFieldU64>(data)->Value);
            case ScriptPrimitiveType::Float: return ScriptValue::Float(StaticRefCast<SerializableFieldFloat>(data)->Value);
            case ScriptPrimitiveType::Double: return ScriptValue::Float(StaticRefCast<SerializableFieldDouble>(data)->Value);
            case ScriptPrimitiveType::String: {
                const auto field = StaticRefCast<SerializableFieldString>(data);
                return field->Null ? ScriptValue::Null() : ScriptValue::Text(field->Value);
            }
            case ScriptPrimitiveType::Vector2: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Vector2;
                const glm::vec2 value = StaticRefCast<SerializableFieldVector2>(data)->Value;
                result.VectorValue = glm::vec4(value, 0.0f, 0.0f);
                return result;
            }
            case ScriptPrimitiveType::Vector3: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Vector3;
                result.VectorValue = glm::vec4(StaticRefCast<SerializableFieldVector3>(data)->Value, 0.0f);
                return result;
            }
            case ScriptPrimitiveType::Vector4: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Vector4;
                result.VectorValue = StaticRefCast<SerializableFieldVector4>(data)->Value;
                return result;
            }
            case ScriptPrimitiveType::Color: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Vector4;
                result.VectorValue = StaticRefCast<SerializableFieldColor>(data)->Value;
                return result;
            }
            case ScriptPrimitiveType::Matrix4: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Matrix4;
                result.MatrixValue = StaticRefCast<SerializableFieldMatrix4>(data)->Value;
                return result;
            }
            case ScriptPrimitiveType::Count: return ScriptValue::Null();
            }
            return ScriptValue::Null();
        }

        ScriptValue ReadLegacyValue(const Ref<SerializableFieldData>& data, const Ref<SerializableTypeInfo>& typeInfo)
        {
            if (data == nullptr || typeInfo == nullptr)
                return ScriptValue::Null();
            switch (typeInfo->GetType())
            {
            case SerializableType::Primitive:
                return ReadLegacyPrimitive(data, StaticRefCast<SerializableTypeInfoPrimitive>(typeInfo)->m_Type);
            case SerializableType::Enum: {
                const auto info = StaticRefCast<SerializableTypeInfoEnum>(typeInfo);
                ScriptValue result = ReadLegacyPrimitive(data, info->m_UnderlyingType);
                result.Kind = ScriptValueKind::Enum;
                if (result.UnsignedValue != 0)
                    result.SignedValue = static_cast<int64_t>(result.UnsignedValue);
                result.DeclaredType = { info->m_AssemblyName, info->m_TypeNamespace, info->m_TypeName };
                return result;
            }
            case SerializableType::Entity: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Entity;
                const Entity entity = StaticRefCast<SerializableFieldEntity>(data)->Value;
                result.ReferenceValue = entity ? entity.GetUuid() : UUID::EMPTY;
                return result;
            }
            case SerializableType::Asset: {
                ScriptValue result;
                result.Kind = ScriptValueKind::Asset;
                const AssetHandle<Asset>& asset = StaticRefCast<SerializableFieldAsset>(data)->Value;
                result.ReferenceValue = asset.HasUUID() ? asset.GetUUID() : UUID::EMPTY;
                return result;
            }
            case SerializableType::Object: {
                const auto field = StaticRefCast<SerializableFieldObject>(data);
                if (field->Value == nullptr)
                    return ScriptValue::Null();
                Ref<SerializableObjectInfo> objectInfo;
                const auto objectType = StaticRefCast<SerializableTypeInfoObject>(typeInfo);
                if (ScriptInfoManager::IsStartedUp())
                    ScriptInfoManager::Get().GetSerializableObjectInfo(*objectType, objectInfo);
                if (objectInfo == nullptr)
                    objectInfo = field->Value->GetObjectInfo();
                return ReadLegacyObject(field->Value, objectInfo);
            }
            case SerializableType::Array:
            case SerializableType::List: {
                const bool array = typeInfo->GetType() == SerializableType::Array;
                const Ref<SerializableTypeInfo> elementType =
                  array ? StaticRefCast<SerializableTypeInfoArray>(typeInfo)->m_ElementType
                        : StaticRefCast<SerializableTypeInfoList>(typeInfo)->m_ElementType;
                Vector<Ref<SerializableFieldData>> values;
                bool isNull = true;
                if (array)
                {
                    const auto field = StaticRefCast<SerializableFieldArray>(data);
                    values = field->Values;
                    isNull = field->Null;
                }
                else
                {
                    const auto field = StaticRefCast<SerializableFieldList>(data);
                    values = field->Values;
                    isNull = field->Null;
                }
                if (isNull)
                    return ScriptValue::Null();
                ScriptValue result;
                result.Kind = array ? ScriptValueKind::Array : ScriptValueKind::List;
                result.Elements.reserve(values.size());
                for (const Ref<SerializableFieldData>& value : values)
                    result.Elements.push_back(ReadLegacyValue(value, elementType));
                return result;
            }
            case SerializableType::Dictionary: return ScriptValue::Null();
            }
            return ScriptValue::Null();
        }

        template <typename T, typename Value> Ref<SerializableFieldData> MakeField(Value value)
        {
            Ref<T> field = CreateRef<T>();
            field->Value = static_cast<decltype(field->Value)>(value);
            return field;
        }

        Ref<SerializableFieldData> WriteLegacyValue(const ScriptValue& value, const Ref<SerializableTypeInfo>& typeInfo);

        Ref<SerializableObject> WriteLegacyObject(const ScriptValue& value, const Ref<SerializableObjectInfo>& objectInfo)
        {
            if (value.Kind == ScriptValueKind::Null)
                return nullptr;
            if (value.Kind != ScriptValueKind::Object || objectInfo == nullptr)
                return nullptr;
            Ref<SerializableObject> object = CreateRef<SerializableObject>(objectInfo);
            for (Ref<SerializableObjectInfo> type = objectInfo; type != nullptr; type = type->m_BaseClass)
            {
                for (const auto& [id, member] : type->m_Fields)
                {
                    if (!member->IsSerializable())
                        continue;
                    const auto source = value.Members.find(member->m_Name);
                    if (source == value.Members.end())
                        continue;
                    Ref<SerializableFieldData> field = WriteLegacyValue(source->second, member->m_TypeInfo);
                    if (field != nullptr)
                        object->SetFieldData(member, field);
                }
            }
            return object;
        }

        Ref<SerializableFieldData> WriteLegacyPrimitive(const ScriptValue& value, ScriptPrimitiveType type)
        {
            switch (type)
            {
            case ScriptPrimitiveType::Bool: return MakeField<SerializableFieldBool>(value.BooleanValue);
            case ScriptPrimitiveType::Char:
                return MakeField<SerializableFieldChar>(value.StringValue.empty() ? '\0' : value.StringValue.front());
            case ScriptPrimitiveType::I8: return MakeField<SerializableFieldI8>(value.SignedValue);
            case ScriptPrimitiveType::U8: return MakeField<SerializableFieldU8>(value.UnsignedValue);
            case ScriptPrimitiveType::I16: return MakeField<SerializableFieldI16>(value.SignedValue);
            case ScriptPrimitiveType::U16: return MakeField<SerializableFieldU16>(value.UnsignedValue);
            case ScriptPrimitiveType::I32: return MakeField<SerializableFieldI32>(value.SignedValue);
            case ScriptPrimitiveType::U32: return MakeField<SerializableFieldU32>(value.UnsignedValue);
            case ScriptPrimitiveType::I64: return MakeField<SerializableFieldI64>(value.SignedValue);
            case ScriptPrimitiveType::U64: return MakeField<SerializableFieldU64>(value.UnsignedValue);
            case ScriptPrimitiveType::Float: return MakeField<SerializableFieldFloat>(value.FloatingValue);
            case ScriptPrimitiveType::Double: return MakeField<SerializableFieldDouble>(value.FloatingValue);
            case ScriptPrimitiveType::String: {
                Ref<SerializableFieldString> field = CreateRef<SerializableFieldString>();
                field->Null = value.Kind == ScriptValueKind::Null;
                field->Value = value.StringValue;
                return field;
            }
            case ScriptPrimitiveType::Vector2: {
                Ref<SerializableFieldVector2> field = CreateRef<SerializableFieldVector2>();
                field->Value = glm::vec2(value.VectorValue);
                return field;
            }
            case ScriptPrimitiveType::Vector3: {
                Ref<SerializableFieldVector3> field = CreateRef<SerializableFieldVector3>();
                field->Value = glm::vec3(value.VectorValue);
                return field;
            }
            case ScriptPrimitiveType::Vector4: return MakeField<SerializableFieldVector4>(value.VectorValue);
            case ScriptPrimitiveType::Color: return MakeField<SerializableFieldColor>(value.VectorValue);
            case ScriptPrimitiveType::Matrix4: return MakeField<SerializableFieldMatrix4>(value.MatrixValue);
            case ScriptPrimitiveType::Count: return nullptr;
            }
            return nullptr;
        }

        Ref<SerializableFieldData> WriteLegacyValue(const ScriptValue& value, const Ref<SerializableTypeInfo>& typeInfo)
        {
            if (typeInfo == nullptr)
                return nullptr;
            switch (typeInfo->GetType())
            {
            case SerializableType::Primitive:
                return WriteLegacyPrimitive(value, StaticRefCast<SerializableTypeInfoPrimitive>(typeInfo)->m_Type);
            case SerializableType::Enum: {
                const auto info = StaticRefCast<SerializableTypeInfoEnum>(typeInfo);
                ScriptValue underlying = value;
                if (GetPrimitiveKind(info->m_UnderlyingType) == ScriptValueKind::UnsignedInteger)
                    underlying.UnsignedValue = static_cast<uint64_t>(value.SignedValue);
                return WriteLegacyPrimitive(underlying, info->m_UnderlyingType);
            }
            case SerializableType::Entity: {
                Ref<SerializableFieldEntity> field = CreateRef<SerializableFieldEntity>();
                Scene* scene = SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveScene().get() : nullptr;
                if (scene != nullptr && value.ReferenceValue != UUID::EMPTY)
                    field->Value = scene->TryGetEntityFromUuid(value.ReferenceValue);
                return field;
            }
            case SerializableType::Asset: {
                Ref<SerializableFieldAsset> field = CreateRef<SerializableFieldAsset>();
                if (AssetManager::TryGet() != nullptr && value.ReferenceValue != UUID::EMPTY)
                    field->Value = AssetManager::TryGet()->GetAssetHandle(value.ReferenceValue);
                return field;
            }
            case SerializableType::Object: {
                Ref<SerializableFieldObject> field = CreateRef<SerializableFieldObject>();
                const auto objectType = StaticRefCast<SerializableTypeInfoObject>(typeInfo);
                Ref<SerializableObjectInfo> objectInfo;
                if (ScriptInfoManager::IsStartedUp())
                    ScriptInfoManager::Get().GetSerializableObjectInfo(*objectType, objectInfo);
                field->Value = WriteLegacyObject(value, objectInfo);
                field->AllowNull = !objectType->m_ValueType;
                return field;
            }
            case SerializableType::Array:
            case SerializableType::List: {
                const bool array = typeInfo->GetType() == SerializableType::Array;
                const Ref<SerializableTypeInfo> elementType =
                  array ? StaticRefCast<SerializableTypeInfoArray>(typeInfo)->m_ElementType
                        : StaticRefCast<SerializableTypeInfoList>(typeInfo)->m_ElementType;
                if (array)
                {
                    Ref<SerializableFieldArray> field = CreateRef<SerializableFieldArray>();
                    field->ElementType = elementType;
                    field->Null = value.Kind == ScriptValueKind::Null;
                    for (const ScriptValue& element : value.Elements)
                        field->Values.push_back(WriteLegacyValue(element, elementType));
                    return field;
                }
                Ref<SerializableFieldList> field = CreateRef<SerializableFieldList>();
                field->ElementType = elementType;
                field->Null = value.Kind == ScriptValueKind::Null;
                for (const ScriptValue& element : value.Elements)
                    field->Values.push_back(WriteLegacyValue(element, elementType));
                return field;
            }
            case SerializableType::Dictionary: return nullptr;
            }
            return nullptr;
        }

        Entity ResolveEntity(const UUID& uuid)
        {
            if (uuid == UUID::EMPTY || SceneManager::TryGet() == nullptr)
                return {};
            const Ref<Scene>& scene = SceneManager::TryGet()->GetActiveScene();
            return scene != nullptr ? scene->TryGetEntityFromUuid(uuid) : Entity{};
        }

        MonoScript* FindScript(const UUID& entityId, uint64_t runtimeInstanceId, const ScriptTypeIdentity* identity = nullptr)
        {
            Entity entity = ResolveEntity(entityId);
            if (!entity || !entity.HasComponent<MonoScriptComponent>())
                return nullptr;
            MonoScriptComponent& component = entity.GetComponent<MonoScriptComponent>();
            if (runtimeInstanceId != 0)
                return component.FindScript(runtimeInstanceId);
            const auto script = std::find_if(component.Scripts.begin(), component.Scripts.end(),
                                             [&](MonoScript& candidate) { return identity != nullptr && candidate.GetTypeIdentity() == *identity; });
            return script == component.Scripts.end() ? nullptr : &*script;
        }

        Ref<SerializableObjectInfo> ResolveObjectInfo(MonoScript& script, const PersistedScriptState& persisted)
        {
            Ref<SerializableObjectInfo> objectInfo = script.GetObjectInfo();
            if (objectInfo == nullptr)
                ScriptInfoManager::Get().GetSerializableObjectInfo(script.GetAssemblyName(), script.GetNamespace(), script.GetTypeName(), objectInfo);
            if (objectInfo == nullptr && persisted.Fields != nullptr)
                objectInfo = persisted.Fields->GetObjectInfo();
            return objectInfo;
        }

        ManagedOperationResult Failure(String code, String message)
        {
            return ManagedOperationResult::Failure(std::move(code), std::move(message), ManagedBackendId::Mono);
        }

        class MonoBackend final : public ManagedBackend
        {
        public:
            ManagedOperationResult Start(const ManagedScriptingConfig& config) override
            {
                if (config.ExecutionMode != ManagedExecutionMode::Interpreter && config.ExecutionMode != ManagedExecutionMode::Jit)
                    return Failure("managed.mono.execution_mode", "Mono supports interpreter and JIT execution modes.");
                if (g_MonoHostApi != nullptr)
                    return Failure("managed.mono.already_started", "Another Mono managed backend is already running.");
                if (!MonoManager::IsStartedUp())
                {
                    const MonoRuntimePaths paths = config.RuntimeRoot.empty() ? ResolveMonoRuntimePaths(Path("."))
                                                                               : ResolveMonoRuntimePaths(Vector<Path>{ config.RuntimeRoot });
                    if (!paths.HasRuntime())
                        return Failure("managed.mono.runtime_missing", "Mono requires a complete runtime root.");
                    MonoManager::StartUp(paths.LibraryDirectory, paths.EtcDirectory, config.EnableDebugging ? MONO_DEBUG_PORT : 0);
                    m_OwnsMono = true;
                }
                m_Config = config;
                m_HostApi = {};
                m_HostApi.size = sizeof(m_HostApi);
                m_HostApi.abi_version = CW_MANAGED_ABI_VERSION;
                m_HostApi.context = this;
                PopulateManagedHostBindings(m_HostApi);
                g_MonoHostApi = &m_HostApi;
                m_Started = true;
                return ManagedOperationResult::Success();
            }

            void Shutdown() override
            {
                for (const auto& [handle, instance] : m_Instances)
                    DestroyRuntimeInstance(instance);
                m_Instances.clear();
                m_Catalog = {};
                m_CurrentProgram = {};
                m_ProgramLoaded = false;
                if (g_MonoHostApi == &m_HostApi)
                    g_MonoHostApi = nullptr;
                m_HostApi = {};
                if (m_OwnsScriptAssets)
                    ScriptAssetManager::Shutdown();
                if (m_OwnsScriptObjects)
                    ScriptObjectManager::Shutdown();
                if (m_OwnsSceneObjects && ScriptSceneObjectManager::IsStartedUp())
                {
                    ScriptSceneObjectManager::Get().Del();
                    ScriptSceneObjectManager::Shutdown();
                }
                if (m_OwnsScriptInfo)
                    ScriptInfoManager::Shutdown();
                if (m_OwnsMono)
                    MonoManager::Shutdown();
                m_OwnsScriptAssets = false;
                m_OwnsScriptObjects = false;
                m_OwnsSceneObjects = false;
                m_OwnsScriptInfo = false;
                m_OwnsMono = false;
                m_Started = false;
                m_NextHandle = 1;
            }

            ManagedCapabilities GetCapabilities() const override
            {
                ManagedCapabilities capabilities;
                capabilities.DynamicProgramLoading = true;
                capabilities.Reload = true;
                capabilities.RuntimeReflection = true;
                capabilities.ManagedDebugging = m_Config.EnableDebugging;
                capabilities.Profiling = m_Config.EnableProfiling;
                capabilities.Threads = true;
                capabilities.NativeDynamicLibraries = true;
                return capabilities;
            }

            ManagedOperationResult LoadProgram(const ManagedProgramDefinition& program) override
            {
                if (!m_Started)
                    return Failure("managed.mono.not_started", "The Mono adapter is not running.");
                if (m_ProgramLoaded)
                    return Failure("managed.mono.program_already_loaded", "Unload or reload the current Mono program first.");
                EnsureServices();
                ManagedOperationResult loaded = LoadAssemblies(program);
                if (!loaded.Succeeded)
                    return loaded;
                m_Catalog = BuildCatalog();
                if (ManagedOperationResult valid = ValidateScriptCatalog(m_Catalog, ManagedBackendId::Mono); !valid.Succeeded)
                    return valid;
                m_CurrentProgram = program;
                m_ProgramLoaded = true;
                return ManagedOperationResult::Success();
            }

            ManagedBackendReloadResult ReloadProgram(const ManagedProgramDefinition& program,
                                                     const Vector<ManagedBackendReloadInstance>& snapshots) override
            {
                if (!m_ProgramLoaded)
                    return { Failure("managed.mono.program_not_loaded", "No Mono program is loaded."), {} };
                const ManagedProgramDefinition previous = m_CurrentProgram;
                const AssemblyRefreshResult replacement = RefreshAssemblies(program);
                if (!replacement.Succeeded())
                {
                    ManagedBackendReloadResult failure = MonoBackendDetail::BuildAssemblyRefreshFailure(replacement);
                    if (failure.ProgramInvalidated)
                        InvalidateProgram();
                    return failure;
                }
                ScriptCatalog replacementCatalog = BuildCatalog();
                ManagedOperationResult valid = ValidateScriptCatalog(replacementCatalog, ManagedBackendId::Mono);
                if (valid.Succeeded)
                    valid = RestoreSnapshots(snapshots, replacementCatalog);
                if (!valid.Succeeded)
                {
                    const bool assembliesRestored = RefreshAssemblies(previous).Succeeded();
                    if (!assembliesRestored)
                    {
                        ManagedBackendReloadResult failure =
                          MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(valid), false, ManagedOperationResult::Success());
                        if (failure.ProgramInvalidated)
                            InvalidateProgram();
                        return failure;
                    }
                    else
                    {
                        m_Catalog = BuildCatalog();
                        ManagedOperationResult stateRestoration = RestoreSnapshots(snapshots, m_Catalog);
                        if (!stateRestoration.Succeeded)
                        {
                            ManagedBackendReloadResult failure =
                              MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(valid), true, stateRestoration);
                            if (failure.ProgramInvalidated)
                                InvalidateProgram();
                            return failure;
                        }
                    }
                    return { std::move(valid), {} };
                }
                m_Catalog = std::move(replacementCatalog);
                m_CurrentProgram = program;
                Vector<uint64_t> handles;
                handles.reserve(snapshots.size());
                for (const ManagedBackendReloadInstance& snapshot : snapshots)
                    handles.push_back(snapshot.PreviousHandle);
                return { ManagedOperationResult::Success(), std::move(handles) };
            }

            const ScriptCatalog& GetScriptCatalog() const override { return m_Catalog; }

            ManagedBackendCreateResult CreateScript(const ScriptCreateRequest& request) override
            {
                if (!m_ProgramLoaded)
                    return { Failure("managed.mono.program_not_loaded", "No Mono program is loaded."), 0 };
                const ScriptTypeSchema* schema = m_Catalog.FindType(request.Identity);
                if (schema == nullptr)
                    return { Failure("managed.script.type_missing", "The Mono script type is not in the catalog."), 0 };
                Entity entity = ResolveEntity(request.Entity);
                if (!entity)
                    return { Failure("managed.mono.entity_missing", "The script entity is no longer in the active scene."), 0 };
                bool componentAdded = false;
                bool occurrenceAdded = false;
                bool createAttempted = false;
                MonoScript* script = FindScript(request.Entity, request.RuntimeInstanceId, &request.Identity);
                if (script == nullptr && request.RuntimeInstanceId == 0)
                {
                    componentAdded = !entity.HasComponent<MonoScriptComponent>();
                    MonoScriptComponent& component =
                      componentAdded ? entity.AddComponent<MonoScriptComponent>() : entity.GetComponent<MonoScriptComponent>();
                    component.Scripts.emplace_back(request.Identity);
                    script = &component.Scripts.back();
                    occurrenceAdded = true;
                }
                if (script == nullptr)
                    return { Failure("managed.mono.script_occurrence_missing", "The persisted Mono script occurrence was not found."), 0 };
                const uint64_t runtimeInstanceId = script->InstanceId;
                const auto rollbackCreate = [&]() {
                    if (!occurrenceAdded && createAttempted && ScriptSceneObjectManager::IsStartedUp())
                        ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, script);
                    MonoBackendDetail::RollbackAddedScriptOccurrence(entity, runtimeInstanceId, occurrenceAdded, componentAdded);
                };
                if (m_NextHandle == 0)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.handle_exhausted", "Mono script handles are exhausted."), 0 };
                }
                if (std::any_of(m_Instances.begin(), m_Instances.end(), [&](const auto& entry) {
                        return entry.second.Entity == request.Entity && entry.second.RuntimeInstanceId == runtimeInstanceId;
                    }))
                {
                    rollbackCreate();
                    return { Failure("managed.mono.script_occurrence_active", "The persisted Mono script occurrence is already active."), 0 };
                }
                createAttempted = true;
                script->Create(entity);
                if (script->GetManagedInstance() == nullptr)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.create_failed", "Mono could not create the managed script instance."), 0 };
                }
                Instance instance;
                instance.Entity = request.Entity;
                instance.RuntimeInstanceId = runtimeInstanceId;
                if (request.InitialState.Identity.IsValid())
                {
                    ScriptStateResult migrated = MigrateScriptState(request.InitialState, *schema, ManagedBackendId::Mono);
                    if (!migrated.Result.Succeeded)
                    {
                        rollbackCreate();
                        return { std::move(migrated.Result), 0 };
                    }
                    ManagedOperationResult applied = ApplyState(*script, migrated.State);
                    if (!applied.Succeeded)
                    {
                        rollbackCreate();
                        return { std::move(applied), 0 };
                    }
                    instance.OrphanedMembers = std::move(migrated.State.OrphanedMembers);
                }
                const uint64_t handle = m_NextHandle++;
                m_Instances.emplace(handle, std::move(instance));
                return { ManagedOperationResult::Success(), handle };
            }

            ManagedOperationResult DestroyScript(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                DestroyRuntimeInstance(instance->second);
                m_Instances.erase(instance);
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult Dispatch(uint64_t handle, const ScriptEvent& event) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                MonoScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                if (script == nullptr || script->GetManagedInstance() == nullptr)
                    return StaleHandle();
                Entity self = ResolveEntity(instance->second.Entity);
                Entity other = ResolveEntity(event.OtherEntity);
                switch (event.Kind)
                {
                case ScriptEventKind::Start: script->OnStart(); break;
                case ScriptEventKind::Update: script->OnUpdate(); break;
                case ScriptEventKind::Destroy: script->OnDestroy(); break;
                case ScriptEventKind::CollisionEnter2D:
                case ScriptEventKind::CollisionStay2D:
                case ScriptEventKind::CollisionExit2D: {
                    Collision2D collision;
                    collision.Colliders = { self, other };
                    for (const ScriptContactPoint& contact : event.Contacts)
                        collision.Points.emplace_back(contact.Position.x, contact.Position.y);
                    if (event.Kind == ScriptEventKind::CollisionEnter2D)
                        script->OnCollisionEnter2D(collision);
                    else if (event.Kind == ScriptEventKind::CollisionStay2D)
                        script->OnCollisionStay2D(collision);
                    else
                        script->OnCollisionExit2D(collision);
                    break;
                }
                case ScriptEventKind::TriggerEnter2D: script->OnTriggerEnter2D(other); break;
                case ScriptEventKind::TriggerStay2D: script->OnTriggerStay2D(other); break;
                case ScriptEventKind::TriggerExit2D: script->OnTriggerExit2D(other); break;
                case ScriptEventKind::CollisionEnter3D:
                case ScriptEventKind::CollisionStay3D:
                case ScriptEventKind::CollisionExit3D: {
                    Collision3D collision;
                    collision.Colliders = { self, other };
                    for (const ScriptContactPoint& contact : event.Contacts)
                        collision.Points.push_back({ contact.Position, contact.Normal, contact.Separation, contact.Impulse });
                    if (event.Kind == ScriptEventKind::CollisionEnter3D)
                        script->OnCollisionEnter3D(collision);
                    else if (event.Kind == ScriptEventKind::CollisionStay3D)
                        script->OnCollisionStay3D(collision);
                    else
                        script->OnCollisionExit3D(collision);
                    break;
                }
                case ScriptEventKind::TriggerEnter3D: script->OnTriggerEnter3D(other); break;
                case ScriptEventKind::TriggerStay3D: script->OnTriggerStay3D(other); break;
                case ScriptEventKind::TriggerExit3D: script->OnTriggerExit3D(other); break;
                }
                return ManagedOperationResult::Success();
            }

            ManagedBackendStateResult CaptureState(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return { StaleHandle(), {} };
                MonoScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                if (script == nullptr)
                    return { StaleHandle(), {} };
                const PersistedScriptState persisted = script->CapturePersistedState();
                const Ref<SerializableObjectInfo> objectInfo = ResolveObjectInfo(*script, persisted);
                if (persisted.Fields == nullptr || objectInfo == nullptr)
                    return { Failure("managed.mono.state_unavailable", "Mono could not capture the script's reflected state."), {} };
                ScriptState state;
                state.Identity = script->GetTypeIdentity();
                state.Root = ReadLegacyObject(persisted.Fields, objectInfo);
                state.OrphanedMembers = instance->second.OrphanedMembers;
                return { ManagedOperationResult::Success(), std::move(state) };
            }

            ManagedOperationResult ApplyState(uint64_t handle, const ScriptState& state) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                MonoScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                if (script == nullptr)
                    return StaleHandle();
                ManagedOperationResult result = ApplyState(*script, state);
                if (result.Succeeded)
                    instance->second.OrphanedMembers = state.OrphanedMembers;
                return result;
            }

            Vector<ManagedDiagnostic> Update() override
            {
                if (ScriptObjectManager::IsStartedUp())
                    ScriptObjectManager::Get().Update();
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneManager::DispatchPendingEvents();
                return {};
            }

            void NotifyEntityDestroyed(const Entity& entity) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().NotifyEntityDestroyed(entity);
            }

            void NotifyComponentDestroyed(uint64_t instanceId) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().NotifyComponentDestroyed(instanceId);
            }

            void NotifySceneDestroyed(const Scene* scene) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().DestroySceneObjects(scene);
            }

            void NotifySceneEventsAvailable() override { ScriptSceneManager::DispatchPendingEvents(); }

        private:
            struct Instance
            {
                UUID Entity;
                uint64_t RuntimeInstanceId = 0;
                Map<String, ScriptValue> OrphanedMembers;
            };

            template <typename T> static void StartModule(bool& owned)
            {
                if (!T::IsStartedUp())
                {
                    T::StartUp();
                    owned = true;
                }
            }

            void EnsureServices()
            {
                ScriptBindings::Register();
                StartModule<ScriptInfoManager>(m_OwnsScriptInfo);
                StartModule<ScriptSceneObjectManager>(m_OwnsSceneObjects);
                StartModule<ScriptObjectManager>(m_OwnsScriptObjects);
                StartModule<ScriptAssetManager>(m_OwnsScriptAssets);
            }

            ManagedOperationResult LoadAssemblies(const ManagedProgramDefinition& program)
            {
                const ManagedProgramArtifact* engine = FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly);
                const ManagedProgramArtifact* game = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly);
                if (engine == nullptr || !fs::is_regular_file(engine->Filepath))
                    return Failure("managed.mono.engine_assembly_missing", "Mono requires the CrownySharp engine assembly.");
                try
                {
                    MonoAssembly* loadedEngine = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
                    if (loadedEngine == nullptr || !loadedEngine->IsLoaded())
                        loadedEngine = &MonoManager::Get().LoadAssembly(engine->Filepath, CROWNY_ASSEMBLY);
                    MonoClass* runtimeContext = loadedEngine->GetClass(CROWNY_NS, "ManagedRuntimeContext");
                    if (runtimeContext == nullptr)
                        return Failure("managed.mono.host_api_missing", "CrownySharp does not contain the managed runtime context.");
                    runtimeContext->AddInternalCall("Internal_GetNativeHostApi", reinterpret_cast<const void*>(&GetMonoHostApi));
                    ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
                    if (game != nullptr && fs::is_regular_file(game->Filepath))
                    {
                        MonoAssembly* loadedGame = MonoManager::Get().GetAssembly(GAME_ASSEMBLY);
                        if (loadedGame == nullptr || !loadedGame->IsLoaded())
                            MonoManager::Get().LoadAssembly(game->Filepath, GAME_ASSEMBLY);
                        ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
                    }
                }
                catch (const std::exception& error)
                {
                    return Failure("managed.mono.assembly_load_failed", String("Mono could not load the managed program: ") + error.what());
                }
                return ManagedOperationResult::Success();
            }

            Vector<AssemblyRefreshInfo> GetRefreshAssemblies(const ManagedProgramDefinition& program) const
            {
                Vector<AssemblyRefreshInfo> assemblies;
                if (const ManagedProgramArtifact* engine = FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly))
                    assemblies.emplace_back(CROWNY_ASSEMBLY, engine->Filepath);
                if (const ManagedProgramArtifact* game = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly))
                    assemblies.emplace_back(GAME_ASSEMBLY, game->Filepath);
                return assemblies;
            }

            AssemblyRefreshResult RefreshAssemblies(const ManagedProgramDefinition& program)
            {
                const Vector<AssemblyRefreshInfo> assemblies = GetRefreshAssemblies(program);
                return assemblies.empty() ? AssemblyRefreshResult{ AssemblyRefreshStatus::CurrentDomainKept }
                                          : ScriptObjectManager::Get().RefreshAssemblies(assemblies);
            }

            ManagedOperationResult RestoreSnapshots(const Vector<ManagedBackendReloadInstance>& snapshots, const ScriptCatalog& catalog)
            {
                for (const ManagedBackendReloadInstance& snapshot : snapshots)
                {
                    const auto instance = m_Instances.find(snapshot.PreviousHandle);
                    if (instance == m_Instances.end())
                        return StaleHandle();
                    const ScriptTypeSchema* schema = catalog.FindType(snapshot.State.Identity);
                    if (schema == nullptr)
                        return Failure("managed.mono.reload_type_missing", "A live Mono script type is missing from the replacement program.");
                    ScriptStateResult migrated = MigrateScriptState(snapshot.State, *schema, ManagedBackendId::Mono);
                    if (!migrated.Result.Succeeded)
                        return migrated.Result;
                    MonoScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                    if (script == nullptr)
                        return StaleHandle();
                    ManagedOperationResult applied = ApplyState(*script, migrated.State);
                    if (!applied.Succeeded)
                        return applied;
                    instance->second.OrphanedMembers = std::move(migrated.State.OrphanedMembers);
                }
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult ApplyState(MonoScript& script, const ScriptState& state)
            {
                if (state.Identity != script.GetTypeIdentity())
                    return Failure("managed.mono.state_identity_mismatch", "The script state identity does not match the Mono script type.");
                const PersistedScriptState previous = script.CapturePersistedState();
                Ref<SerializableObjectInfo> objectInfo = ResolveObjectInfo(script, previous);
                if (objectInfo == nullptr)
                    return Failure("managed.mono.state_schema_missing", "Mono reflection metadata for the script state is unavailable.");
                Ref<SerializableObject> fields = WriteLegacyObject(state.Root, objectInfo);
                if (fields == nullptr || !script.ApplyPersistedState({ state.Identity, fields }))
                {
                    script.ApplyPersistedState(previous);
                    return Failure("managed.mono.state_apply_failed", "Mono could not apply the script state.");
                }
                return ManagedOperationResult::Success();
            }

            void DestroyRuntimeInstance(const Instance& instance)
            {
                Entity entity = ResolveEntity(instance.Entity);
                MonoScript* script = FindScript(instance.Entity, instance.RuntimeInstanceId);
                if (entity && script != nullptr)
                {
                    if (ScriptSceneObjectManager::IsStartedUp())
                        ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, script);
                    script->ClearRuntimeHandle();
                }
            }

            void InvalidateProgram()
            {
                for (const auto& [handle, instance] : m_Instances)
                    DestroyRuntimeInstance(instance);
                m_Instances.clear();
                m_Catalog = {};
                m_CurrentProgram = {};
                m_ProgramLoaded = false;
            }

            ManagedOperationResult StaleHandle() const
            {
                return Failure("managed.mono.stale_handle", "The Mono script handle is stale.");
            }

            ManagedScriptingConfig m_Config;
            ManagedProgramDefinition m_CurrentProgram;
            ScriptCatalog m_Catalog;
            cw_managed_host_api m_HostApi{};
            Map<uint64_t, Instance> m_Instances;
            uint64_t m_NextHandle = 1;
            bool m_Started = false;
            bool m_ProgramLoaded = false;
            bool m_OwnsMono = false;
            bool m_OwnsScriptInfo = false;
            bool m_OwnsSceneObjects = false;
            bool m_OwnsScriptObjects = false;
            bool m_OwnsScriptAssets = false;
        };
    } // namespace

    ScriptState ConvertLegacyScriptState(const PersistedScriptState& persisted)
    {
        ScriptState state;
        if (!persisted.Identity.IsValid() || persisted.Fields == nullptr || persisted.Fields->GetObjectInfo() == nullptr)
            return state;
        state.Identity = persisted.Identity;
        state.Root = ReadLegacyObject(persisted.Fields, persisted.Fields->GetObjectInfo());
        return state;
    }

    ScriptSchemaFieldFlags MonoBackendDetail::GetSchemaFieldFlags(const Ref<SerializableMemberInfo>& member)
    {
        ScriptSchemaFieldFlags flags = ScriptSchemaFieldFlags::None;
        if (member == nullptr)
            return flags;
        if (member->m_Flags.IsSet(ScriptFieldFlagBits::Serializable))
            flags = flags | ScriptSchemaFieldFlags::Serializable;
        if (member->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
            flags = flags | ScriptSchemaFieldFlags::Inspectable;
        if (member->m_Flags.IsSet(ScriptFieldFlagBits::ReadOnly))
            flags = flags | ScriptSchemaFieldFlags::ReadOnly;

        const ScriptValueKind kind = GetValueKind(member->m_TypeInfo);
        bool referenceType = kind == ScriptValueKind::Entity || kind == ScriptValueKind::Asset || kind == ScriptValueKind::Array ||
                             kind == ScriptValueKind::List || kind == ScriptValueKind::Dictionary;
        if (member->m_TypeInfo != nullptr && member->m_TypeInfo->GetType() == SerializableType::Primitive)
            referenceType = StaticRefCast<SerializableTypeInfoPrimitive>(member->m_TypeInfo)->m_Type == ScriptPrimitiveType::String;
        else if (member->m_TypeInfo != nullptr && member->m_TypeInfo->GetType() == SerializableType::Object)
            referenceType = !StaticRefCast<SerializableTypeInfoObject>(member->m_TypeInfo)->m_ValueType;
        if (referenceType && !member->m_Flags.IsSet(ScriptFieldFlagBits::NotNull))
            flags = flags | ScriptSchemaFieldFlags::Nullable;
        return flags;
    }

    void MonoBackendDetail::RollbackAddedScriptOccurrence(Entity entity, uint64_t runtimeInstanceId, bool occurrenceAdded, bool componentAdded)
    {
        if (!occurrenceAdded || !entity || !entity.HasComponent<MonoScriptComponent>())
            return;

        MonoScriptComponent& component = entity.GetComponent<MonoScriptComponent>();
        const auto script = std::find_if(component.Scripts.begin(), component.Scripts.end(),
                                         [runtimeInstanceId](const MonoScript& candidate) { return candidate.InstanceId == runtimeInstanceId; });
        if (script != component.Scripts.end())
        {
            if (ScriptSceneObjectManager::IsStartedUp())
                ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, &*script);
            component.Scripts.erase(script);
        }
        if (componentAdded && component.Scripts.empty())
            entity.RemoveComponent<MonoScriptComponent>();
    }

    ManagedBackendReloadResult MonoBackendDetail::BuildAssemblyRefreshFailure(const AssemblyRefreshResult& refresh)
    {
        switch (refresh.Status)
        {
        case AssemblyRefreshStatus::CurrentDomainKept:
            return { ManagedOperationResult::Failure("managed.mono.reload_failed",
                                                     "Mono rejected the replacement assemblies before unloading the current domain.",
                                                     ManagedBackendId::Mono),
                     {},
                     false };
        case AssemblyRefreshStatus::PreviousDomainRestored:
            return { ManagedOperationResult::Failure("managed.mono.reload_failed",
                                                     "Mono could not load the replacement assemblies; the previous domain was restored.",
                                                     ManagedBackendId::Mono),
                     {},
                     false };
        case AssemblyRefreshStatus::PreviousDomainRestoreFailed:
            return AddReloadRollbackDiagnostics(
              ManagedOperationResult::Failure("managed.mono.reload_failed",
                                              "Mono could not load the replacement assemblies or restore the previous domain.",
                                              ManagedBackendId::Mono),
              false, ManagedOperationResult::Success());
        case AssemblyRefreshStatus::ReplacementLoaded: break;
        }
        return { ManagedOperationResult::Failure("managed.mono.reload_failed", "Mono reload failed unexpectedly.", ManagedBackendId::Mono),
                 {},
                 false };
    }

    ManagedBackendReloadResult MonoBackendDetail::AddReloadRollbackDiagnostics(ManagedOperationResult failure, bool assembliesRestored,
                                                                                const ManagedOperationResult& stateRestoration)
    {
        failure.Succeeded = false;
        const bool programInvalidated = !assembliesRestored || !stateRestoration.Succeeded;
        if (!assembliesRestored)
        {
            failure.Diagnostics.push_back({ ManagedDiagnosticSeverity::Error,
                                            "managed.mono.reload_rollback_failed",
                                            "Mono could not restore the last working assemblies; the managed program was invalidated.",
                                            {},
                                            ManagedBackendId::Mono,
                                            {},
                                            {} });
            return { std::move(failure), {}, programInvalidated };
        }
        if (!stateRestoration.Succeeded)
        {
            failure.Diagnostics.insert(failure.Diagnostics.end(), stateRestoration.Diagnostics.begin(), stateRestoration.Diagnostics.end());
            failure.Diagnostics.push_back(
              { ManagedDiagnosticSeverity::Error,
                "managed.mono.reload_state_rollback_failed",
                "Mono restored the last working assemblies but not all live script state; the managed program was invalidated.",
                {},
                ManagedBackendId::Mono,
                {},
                {} });
        }
        return { std::move(failure), {}, programInvalidated };
    }

    Scope<ManagedBackend> CreateMonoBackend() { return CreateScope<MonoBackend>(); }
} // namespace Crowny
