#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptJSON.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Mono/MonoArray.h"

#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

namespace Crowny
{
	ScriptJSON::ScriptJSON() : ScriptObject() {}

	void ScriptJSON::InitRuntimeData()
	{
		MetaData.ScriptClass->AddInternalCall("Internal_FromJson", (void*)&Internal_FromJSON);
		MetaData.ScriptClass->AddInternalCall("Internal_ToJson", (void*)&Internal_ToJSON);
	}

	static void MarshalAs(MonoObject* object, ScriptPrimitiveType primitiveType, const Ref<SerializableMemberInfo> &field, const rapidjson::Value& jsonValue) {
		if (primitiveType == ScriptPrimitiveType::Float)
		{
			float value = (float)jsonValue.GetDouble();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::Double)
		{
			double value = jsonValue.GetDouble();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::I8)
		{
			int8_t value = (int8_t)jsonValue.GetInt();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::I16)
		{
			int16_t value = (int16_t)jsonValue.GetInt();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::I16)
		{
			int16_t value = (int16_t)jsonValue.GetInt();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::I32)
		{
			int32_t value = (int32_t)jsonValue.GetInt();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::I64)
		{
			int64_t value = (int64_t)jsonValue.GetInt64();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::U16)
		{
			uint8_t value = (uint8_t)jsonValue.GetUint();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::U16)
		{
			uint16_t value = (uint16_t)jsonValue.GetUint();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::U32)
		{
			uint32_t value = (uint32_t)jsonValue.GetUint();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::U64)
		{
			uint64_t value = (uint64_t)jsonValue.GetUint64();
			field->SetValue(object, &value);
		}
		else if (primitiveType == ScriptPrimitiveType::String && jsonValue.IsString())
		{
			MonoString* monoString=MonoUtils::ToMonoString(String(jsonValue.GetString(), jsonValue.GetStringLength()));
			field->SetValue(object, monoString);
		}
	}

	template <typename T>
	static void SetValue(const rapidjson::Value& value, MonoObject* instance, const Ref<SerializableMemberInfo>& memberInfo)
	{
		if (value.IsDouble())
		{
			T nativeValue = (T)value.GetDouble();
			memberInfo->SetValue(instance, &nativeValue);
		}
		else
		{
			T nativeValue = (T)value.GetInt64();
			memberInfo->SetValue(instance, &nativeValue);
		}
	}

	static void NumericFromJson(const rapidjson::Value& value, MonoObject* instance, const Ref<SerializableMemberInfo>& memberInfo)
	{
		const Ref<SerializableTypeInfoPrimitive>  primTypeInfo = std::static_pointer_cast<SerializableTypeInfoPrimitive>(memberInfo->m_TypeInfo);
		switch (primTypeInfo->m_Type)
		{
		case ScriptPrimitiveType::I8: SetValue<int8_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::I16: SetValue<int16_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::I32: SetValue<int32_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::I64: SetValue<int64_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::U8: SetValue<uint8_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::U16: SetValue<uint16_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::U32: SetValue<uint32_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::U64: SetValue<uint64_t>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::Float: SetValue<float>(value, instance, memberInfo); break;
		case ScriptPrimitiveType::Double: SetValue<double>(value, instance, memberInfo); break;
		default: CW_ERROR("Mismatched JSON structure");
		}
	}

	static MonoObject* ObjectFromJson(const rapidjson::Value& value, const Ref<SerializableObjectInfo>& objectInfo)
	{
		if (value.IsObject())
		{
			MonoObject* newInstance = objectInfo->m_MonoClass->CreateInstance();
			for (auto [id, memberInfo] : objectInfo->m_Fields)
			{
				rapidjson::Value::ConstMemberIterator iterFind = value.FindMember(memberInfo->m_Name.c_str());
				// C# structure doesn't have this field.
				if (iterFind == value.MemberEnd())
					continue;

				const Ref<SerializableTypeInfo>& typeInfo = memberInfo->m_TypeInfo;
				if (typeInfo->GetType() == SerializableType::Object)
				{
					Ref<SerializableObjectInfo> objInfo = nullptr;
					Ref<SerializableTypeInfoObject> objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
					// Find object serialize info
					if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace, objTypeInfo->m_TypeName, objInfo))
						memberInfo->SetValue(newInstance, ObjectFromJson(iterFind->value, objInfo));
					return newInstance;
				}
				else if (typeInfo->GetType() == SerializableType::Array)
				{
					CW_ENGINE_ASSERT(iterFind->value.IsArray());
					// Do I create these when loading?
					Ref<SerializableTypeInfoArray> arrayTypeInfo = std::static_pointer_cast<SerializableTypeInfoArray>(typeInfo);
					rapidjson::Value::ConstArray arr = iterFind->value.GetArray();
					ScriptArray monoArray = ScriptArray(arrayTypeInfo->m_ElementType->GetMonoClass(), arr.Size());
					int idx = 0;
					Ref<SerializableObjectInfo> objInfo = nullptr;
					Ref<SerializableTypeInfoObject> objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(arrayTypeInfo->m_ElementType);
					// Find object serialize info
					if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace, objTypeInfo->m_TypeName, objInfo))
					for (const auto& val : arr)
						monoArray.Set(idx, ObjectFromJson(val, objInfo));
					memberInfo->SetValue(newInstance, monoArray.GetInternal());
				}
				else if (typeInfo->GetType() == SerializableType::Primitive)
				{
					Ref<SerializableTypeInfoPrimitive> primTypeInfo = std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);
					if (iterFind->value.IsNumber())
						NumericFromJson(iterFind->value, newInstance, memberInfo);
					else if (iterFind->value.IsBool())
					{
						// TODO: Consider doing this with numeric types, not only bool
						if (primTypeInfo->m_Type != ScriptPrimitiveType::Bool)
						{
							CW_ERROR("Mismatched JSON structure");
							continue;
						}
						bool boolValue = iterFind->value.GetBool();
						memberInfo->SetValue(newInstance, &boolValue);
					}
					else if (iterFind->value.IsString())
					{
						if (primTypeInfo->m_Type != ScriptPrimitiveType::String)
						{
							CW_ERROR("Mismatched JSON structure");
							continue;
						}
						MonoString* monoString = MonoUtils::ToMonoString(String(iterFind->value.GetString(), iterFind->value.GetStringLength()));
						memberInfo->SetValue(newInstance, monoString);
					}
				}
			}
			return newInstance;
		}
		return nullptr;
	}

	MonoObject* ScriptJSON::Internal_FromJSON(MonoString* json, MonoReflectionType* type)
	{
		using namespace rapidjson;

		const String nativeString=MonoUtils::FromMonoString(json);

		Document document;
		document.Parse(nativeString.c_str());
		if (!document.IsObject() && !document.IsArray()) // Should probably throw some stuff here
			return nullptr;

		MonoClass* klass=MonoManager::Get().FindClass(MonoUtils::GetClass(type));
		Ref<SerializableObjectInfo> objectInfo;
		if (!ScriptInfoManager::Get().GetSerializableObjectInfo(klass->GetNamespace(), klass->GetName(), objectInfo))
			return nullptr;
		return ObjectFromJson(document, objectInfo);
	}
	/*
	static void ObjectToJson(rapidjson::Value& cur, Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance, rapidjson::Document& doc)
	{
		rapidjson::Value& newCur=cur.SetObject();
		for (auto [id, field] : objectInfo->m_Fields) {
			rapidjson::Value key(field->m_Name.c_str(), field->m_Name.size());
			cur.AddMember(key, newCur, doc.GetAllocator());
			FieldToJson(newCur, field, instance);
		}
	}

	static void FieldToJson(rapidjson::Value& cur, Ref<SerializableMemberInfo>& field, MonoObject* instance, rapidjson::Document& doc)
	{
		if (field->m_TypeInfo->GetType() == SerializableType::Primitive)
		{
			const ScriptPrimitiveType primitiveType = std::static_pointer_cast<SerializableTypeInfoPrimitive>(field->m_TypeInfo)->m_Type;
			switch (primitiveType)
			{
				case ScriptPrimitiveType::Char:
					cur.SetString((String() + (*(char*)MonoUtils::Unbox(field->GetValue(instance)))).c_str(), 1);
					break;
				case ScriptPrimitiveType::String: {
					const String nativeString = MonoUtils::FromMonoString((MonoString*)field->GetValue(instance));
					cur.SetString(nativeString.c_str(), nativeString.size());
					break;
				}
				case ScriptPrimitiveType::Bool:
					cur.SetBool(*(bool*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::I8:
					cur.SetInt(*(int8_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::I16:
					cur.SetInt(*(int16_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::I32:
					cur.SetInt(*(int32_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::I64:
					cur.SetInt64(*(int64_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::U8:
					cur.SetUint(*(uint8_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::U16:
					cur.SetUint(*(uint16_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::U32:
					cur.SetUint(*(int32_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::U64:
					cur.SetUint64(*(int64_t*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::Float:
					cur.SetDouble(*(float*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::Double:
					cur.SetDouble(*(double*)MonoUtils::Unbox(field->GetValue(instance)));
					break;
				case ScriptPrimitiveType::Vector2: {
					rapidjson::Value& vec=cur.SetObject();
					glm::vec2 value = *(glm::vec2*)MonoUtils::Unbox(field->GetValue(instance));
					vec.AddMember(rapidjson::Value("x"), rapidjson::Value(value.x), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("y"), rapidjson::Value(value.y), doc.GetAllocator());
					break;
				}
				case ScriptPrimitiveType::Vector3:
					rapidjson::Value& vec = cur.SetObject();
					glm::vec3 value = *(glm::vec3*)MonoUtils::Unbox(field->GetValue(instance));
					vec.AddMember(rapidjson::Value("x"), rapidjson::Value(value.x), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("y"), rapidjson::Value(value.y), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("z"), rapidjson::Value(value.z), doc.GetAllocator());
					break;
				case ScriptPrimitiveType::Vector4:
					rapidjson::Value& vec = cur.SetObject();
					glm::vec4 value = *(glm::vec4*)MonoUtils::Unbox(field->GetValue(instance));
					vec.AddMember(rapidjson::Value("x"), rapidjson::Value(value.x), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("y"), rapidjson::Value(value.y), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("z"), rapidjson::Value(value.z), doc.GetAllocator());
					vec.AddMember(rapidjson::Value("w"), rapidjson::Value(value.w), doc.GetAllocator());
					break;
				case ScriptPrimitiveType::Matrix4:
					rapidjson::Value& vec = cur.SetArray();
					glm::mat4 value = *(glm::mat4*)MonoUtils::Unbox(field->GetValue(instance));
					for (uint32_t i = 0; i < 16; i++)
						vec.PushBack(rapidjson::Value(value[i]), doc.GetAllocator());
					break;
			}
		}
	}
	*/

	MonoString* ScriptJSON::Internal_ToJSON(MonoObject* object, bool prettyPrint)
	{
		using namespace rapidjson;

		Document document;
		if (!document.IsObject()) // Should probably throw some stuff here
			return nullptr;

		MonoClass* klass = MonoManager::Get().FindClass(MonoUtils::GetClass(object));
		Ref<SerializableObjectInfo> objectInfo;
		if (!ScriptInfoManager::Get().GetSerializableObjectInfo(klass->GetNamespace(), klass->GetName(), objectInfo))
			return nullptr;
		document.SetObject();
		// ToJson(objectInfo);

		if (prettyPrint) {
			StringBuffer buffer;
			buffer.Clear();
			PrettyWriter<StringBuffer> writer(buffer);
			document.Accept(writer);
			const String result=String(buffer.GetString(), buffer.GetSize());
			return MonoUtils::ToMonoString(result);
		}
		else {
			StringBuffer buffer;
			buffer.Clear();
			Writer<StringBuffer> writer(buffer);
			document.Accept(writer);
			const String result = String(buffer.GetString(), buffer.GetSize());
			return MonoUtils::ToMonoString(result);
		}
	}
}