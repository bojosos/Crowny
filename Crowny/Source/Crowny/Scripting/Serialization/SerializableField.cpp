#include "cwpch.h"

#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{

    Ref<SerializableFieldData> SerializableFieldData::Create(const Ref<SerializableTypeInfo>& typeInfo,
                                                             MonoObject* value, bool allowNull)
    {
        if (typeInfo->GetType() == SerializableType::Primitive || typeInfo->GetType() == SerializableType::Enum)
        {
            ScriptPrimitiveType primitiveType = ScriptPrimitiveType::I32;
            if (typeInfo->GetType() == SerializableType::Primitive)
                primitiveType = static_cast<SerializableTypeInfoPrimitive*>(typeInfo.get())->m_Type;
            else
                primitiveType = static_cast<SerializableTypeInfoEnum*>(typeInfo.get())->m_UnderlyingType;

            switch (primitiveType)
            {
            case ScriptPrimitiveType::Bool: {
                auto fieldData = CreateRef<SerializableFieldBool>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Char: {
                auto fieldData = CreateRef<SerializableFieldChar>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I8: {
                auto fieldData = CreateRef<SerializableFieldI8>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U8: {
                auto fieldData = CreateRef<SerializableFieldU8>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I16: {
                auto fieldData = CreateRef<SerializableFieldI16>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U16: {
                auto fieldData = CreateRef<SerializableFieldU16>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I32: {
                auto fieldData = CreateRef<SerializableFieldI32>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U32: {
                auto fieldData = CreateRef<SerializableFieldU32>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I64: {
                auto fieldData = CreateRef<SerializableFieldI64>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U64: {
                auto fieldData = CreateRef<SerializableFieldU64>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Float: {
                auto fieldData = CreateRef<SerializableFieldFloat>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Double: {
                auto fieldData = CreateRef<SerializableFieldDouble>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::String: {
                MonoString* str = (MonoString*)value;
                auto fieldData = CreateRef<SerializableFieldString>();
                if (value != nullptr)
                    fieldData->Value = MonoUtils::FromMonoString(str);
                else
                    fieldData->Null = true;
                return fieldData;
            }
            default:
                break;
            }
        }
        else if (typeInfo->GetType() == SerializableType::Entity)
        {
            auto fieldData = CreateRef<SerializableFieldEntity>();
            if (value != nullptr)
            {
                ScriptEntity* scriptEntity = ScriptEntity::ToNative(value);
                fieldData->Value = scriptEntity->GetNativeEntity();
            }
            return fieldData;
        }
        else if (typeInfo->GetType() == SerializableType::Array)
        {
            // auto fieldData = CreateRef<SerializableFieldArray>();
            // if (value != nullptr)
            //     fieldData->Value = SerializableFieldArray::CreateFromExisting(value, arrayTypeInfo);
            // else if (!allowNull)
            // {
            //     Vector<uint32_t> sizes(arrayTypeInfo->m_Rank, 0);
            //     fieldData->Value = SerializableFieldArray::CreateNew(arrayTypeInfo, sizes);
            // }
            return nullptr;
        }
        else if (typeInfo->GetType() == SerializableType::List)
        {
            // auto fieldData = CreateRef<SerializableFieldArray>();
            // if (value != nullptr)
            //     fieldData->Value = SerializableFieldArray::CreateFromExisting(value, arrayTypeInfo);
            // else if (!allowNull)
            // {
            //     Vector<uint32_t> sizes(arrayTypeInfo->m_Rank, 0);
            //     fieldData->Value = SerializableFieldArray::CreateNew(arrayTypeInfo, sizes);
            // }
            return nullptr;
        }
        else if (typeInfo->GetType() == SerializableType::Dictionary)
        {
            // auto fieldData = CreateRef<SerializableFieldArray>();
            // if (value != nullptr)
            //     fieldData->Value = SerializableFieldArray::CreateFromExisting(value, arrayTypeInfo);
            // else if (!allowNull)
            // {
            //     Vector<uint32_t> sizes(arrayTypeInfo->m_Rank, 0);
            //     fieldData->Value = SerializableFieldArray::CreateNew(arrayTypeInfo, sizes);
            // }
            return nullptr;
        }
        else if (typeInfo->GetType() == SerializableType::Object)
        {
            auto fieldData = CreateRef<SerializableFieldObject>();
            if (value != nullptr)
                fieldData->Value = SerializableObject::CreateFromMonoObject(value);
            else if (!allowNull)
                fieldData->Value = SerializableObject::CreateNew(std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo));
            return fieldData;
        }
        return nullptr;
    }

	void* SerializableFieldObject::GetValue()
	{
		// TODO: Do more checks and take in object info
		return Value->GetManagedInstance();
	}

	void SerializableFieldObject::Serialize()
    {
        if (Value != nullptr) Value->Serialize();
    }

	void SerializableFieldObject::Deserialize()
	{
		if (Value != nullptr)
		{
			MonoObject* managedInstance = Value->Deserialize();
			Value = SerializableObject::CreateFromMonoObject(managedInstance);
		}
	}

	void SerializableFieldObject::SerializeYAML(YAML::Emitter& out) const{ Value->SerializeYAML(out); }
    void SerializableFieldObject::DeserializeYAML(const YAML::Node& node) {} // Need type info from somewhere for the create call { Value = SerializableObject::CreateNew(m_); Value->DeserializeYAML(node); }


    SerializableFieldKey::SerializableFieldKey(uint32_t typeId, uint32_t fieldId)
		: m_TypeId(typeId), m_FieldIdx(fieldId)
	{
	}

} // namespace Crowny