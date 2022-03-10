#include "cwpch.h"
#if 0
#include "Crowny/Scripting/Serialization/SerializableList.h"

namespace Crowny
{
	SerializableList::SerializableList() { }

	SerializableList::SerializableList(const Ref<SerializableTypeInfoList>& listInfo, MonoObject* managedInstance)
	{
		m_GCHandle = MonoUtils::NewGCHandle(managedInstance, false);
		MonoClass* listClass = MonoManager::Get().FindClass(MonoUtils::GetClass(managedInstance));
		if (listClass == nullptr)
			return;
		InitMonoObjects(listClass);
		m_NumElements = GetLength();
	}

	void SerializableList::SetFieldData(uint32_t idx, const Ref<SerializableFieldData>& data)
	{
		if (m_GCHandle != 0)
		{
			MonoObject* instance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
			SetFieldData(instance, idx, data);
		}
		else
			m_CachedEntries = val;
	}

	void SerializableList::SetFieldData(MonoObject* obj, uint32_t idx, const Ref<SerializableFieldData>& val)
	{
		m_ItemProp->SetIndexed(obj, idx, val->GetValue(m_TypeInfo->m_ElementType));
	}

	void SerializableList::AddFieldData(const Ref<SerializableFieldData>& data)
	{
		MonoObject* instance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
		void* params[1] = { val->GetValue(m_ListInfo->m_ElementType) };
		m_AddMethod->Invoke(instance, params);
	}

	Ref<SerializableFieldData> SerializableList::GetFieldData(uint32_t idx)
	{
		if (m_GCHandle != 0)
		{
			MonoObject* instance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
			MonoObject* obj = m_ItemProp->GetIndex(instance, idx);
			return SerializableFieldData::Create(m_ListInfo->m_ElementType, obj);
		}
		return m_CachedEntries[idx];
	}

	void SerializableList::Resize(uint32_t newSize)
	{
		if (m_GCHandle != 0)
		{
			ScriptArray ar(m_ListInfo->m_ElementType->GetMonoClass(), newSize);
			uint32_t minSize = std::min(m_NumElements, newSize);
			uint32_t dummy = 0;
			void* params[4];
			params[0] = &dummy;
			params[1] = ar->GetInternal();
			params[2] = &dummy;
			params[3] = &minSize;

			m_CopyToMethod->Invoke(GetManagedInstance(), params);
			m_ClearMethod->Invoke(GetManagedInstance(), nullptr);
			params[0] = ar->GetInternal();
			m_AddRangeMethod->Invoke(GetManagedInstance(), params);
		}
		else
			m_CachedEntries.resize(newSize);
		m_NumElements = newSize;
	}

	uint32_t SerializableList::GetLength()
	{
		MonoObject* instance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
		MonoObject* length = m_CountProp->Get(instance);
		if (length == nullptr)
			return 0;
		return *(uint32_t*)MonoUtils::Unbox(length);
	}

	void SerializableList::InitMonoObjects(MonoClass* listClass)
	{
		m_ItemProp = listClass->GetProperty("Item");
		m_CountProp = listClass->GetProperty("Count");
		m_AddMethod = listClass->GetMethod("Add", 1);
		m_AddRangeMethod = listClass->GetMethod("AddRange", 1);
		m_ClearMethod = listClass->GetMethod("Clear");
		m_CopyToMethod = listClass->GetMethod("CopyTo", 4);
	}
}
#endif