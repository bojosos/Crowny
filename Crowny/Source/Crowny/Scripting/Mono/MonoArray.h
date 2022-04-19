#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoClass.h"

namespace Crowny
{
    class ScriptArray
    {
    public:
        ScriptArray(MonoArray* array);
        ScriptArray(::MonoClass* monoClass, uint32_t size);

        ScriptArray Copy() const;
        void SetRaw(uint32_t idx, const uint8_t* data, uint32_t size, uint32_t count = 1);
        uint8_t* GetRaw(uint32_t idx, uint32_t size);

        template <class T> T Get(uint32_t idx);
        template <class T> void Set(uint32_t idx, const T& value);

        uint32_t Size() const;
		uint32_t ElementSize() const;
		::MonoArray* GetInternal() const { return m_Array; }

		void Resize(uint32_t newSize);

		template <class T>
		static ScriptArray Create(uint32_t size);
		::MonoClass* GetElementClass();

    private:
        ::MonoArray* m_Array;
    };

    namespace Detail
    {
        template <class T>
        T ScriptArray_Get(MonoArray* ar, uint32_t idx)
        {
            return *(T*)mono_array_addr_with_size(ar, sizeof(T), idx)
        }
		
		template <class T>
        void ScriptArray_Set(MonoArray* ar, uint32_t idx, const T& value)
        {
			::MonoClass* arrayClass = mono_object_get_class((MonoObject*)(ar));
			::MonoClass* elementClass = mono_class_get_element_class(arrayClass);

			CW_ENGINE_ASSERT((UINT32)mono_class_array_element_size(elementClass) == size);
			CW_ENGINE_ASSERT((idx + count) <= mono_array_length(ar));

			if (mono_class_is_valuetype(elementClass))
				mono_value_copy_array(array, idx, (void*)value, count);
			else
			{
				uint8_t* dest = (uint8_t*)mono_array_addr_with_size(ar, size, idx);
				mono_gc_wbarrier_arrayref_copy(dest, (void*)value, count);
			}
        }

        template <>
		String ScriptArray_Get(MonoArray*, uint32_t idx);
		template <>
		Path ScriptArray_Get(MonoArray*, uint32_t idx);
		template <>
		void ScriptArray_Set<String>(MonoArray* ar, uint32_t idx, const String& string);
		template <>
        void ScriptArray_Set<Path>(MonoArray* ar, uint32_t idx, const Path& path);
		template <>
		void ScriptArray_Set<std::nullptr_t>(MonoArray* ar, uint32_t idx, const nullptr_t& null);

		template <class T>
        inline ScriptArray ScriptArray_Create(uint32_t size)
        {
            return ScriptArray(*T::GetMetaData()->ScriptClass, size);
        }
		
		template <>
        inline ScriptArray ScriptArray_Create<uint8_t>(uint32_t size)
        {
            return ScriptArray(MonoUtils::GetByteClass(), size);
        }

		template <>
		inline ScriptArray ScriptArray_Create<int8_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetSByteClass(), size);
		}
		
		template <>
		inline ScriptArray ScriptArray_Create<char>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetCharClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<int16_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetI16Class(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<int32_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetI32Class(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<uint32_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetU32Class(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<int64_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetI64Class(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<uint64_t>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetU64Class(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<String>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetStringClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<Path>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetStringClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<float>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetFloatClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<double>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetDoubleClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<bool>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetBoolClass(), size);
		}

		template <>
		inline ScriptArray ScriptArray_Create<MonoObject*>(uint32_t size)
		{
			return ScriptArray(MonoUtils::GetObjectClass(), size);
		}

   }

    template <class T>
	T ScriptArray::Get(uint32_t idx)
	{
		/*T value;
		memcpy(&value, GetRaw(idx, sizeof(T)), sizeof(T));
		return value;*/
		return Detail::ScriptArray_Get<T>(m_Array, idx);
	}
	
    template <class T>
	void ScriptArray::Set(uint32_t idx, const T& value)
	{
		// uint8_t* data = GetArrayAddr(sizeof(T), idx);
		// memcpy(data, &value, sizeof(T));
        return Detail::ScriptArray_Get<T>(m_Array, idx, value);
	}

	template<class T>
    ScriptArray ScriptArray::Create(uint32_t size)
    {
        return Detail::ScriptArray_Create<T>(size);
    }
} // namespace Crowny