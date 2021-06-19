#include "cwpch.h"

#include "Crowny/Scripting/CWMonoField.h"
#include "Crowny/Scripting/CWMonoClass.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/class.h>

namespace Crowny
{

	CWMonoField::CWMonoField(MonoClassField* field) : m_Field(field)
	{
		MonoType* type = mono_field_get_type(field);
		MonoClass* classType = mono_class_from_mono_type(type);
		if (classType == nullptr)
			m_Type = nullptr;

		m_Type = new CWMonoClass(classType);
		m_Name = mono_field_get_name(m_Field);
		m_FullDeclName = CWMonoVisibilityToString(GetVisibility()) + (IsStatic() ? " static " : " ") + mono_field_full_name(m_Field);
		
		if (IsStatic())
		{
			m_OwningTypeVTable = mono_class_vtable(CWMonoRuntime::GetDomain(), mono_type_get_class(mono_field_get_type(field)));
			mono_runtime_class_init(m_OwningTypeVTable);
		}
	}

	CWMonoVisibility CWMonoField::GetVisibility() const
	{
		uint32_t flags = mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_FIELD_ACCESS_MASK;
		switch (flags)
		{
			case MONO_FIELD_ATTR_PRIVATE:       return CWMonoVisibility::Private;
			case MONO_FIELD_ATTR_FAM_AND_ASSEM: return CWMonoVisibility::ProtectedInternal;
			case MONO_FIELD_ATTR_ASSEMBLY:      return CWMonoVisibility::Internal;
			case MONO_FIELD_ATTR_FAMILY:        return CWMonoVisibility::Protected;
			case MONO_FIELD_ATTR_PUBLIC:        return CWMonoVisibility::Public;
		}

		CW_ENGINE_ERROR("Unkown mono type");

		return CWMonoVisibility::Private;
	}
	
	void CWMonoField::Set(MonoObject* obj, void* value)
	{
		mono_field_set_value(obj, m_Field, value);
	}

	void CWMonoField::Get(MonoObject* obj, void* outval)
	{
		mono_field_get_value(obj, m_Field, outval);
	}

	MonoObject* CWMonoField::GetBoxed(MonoObject* instance)
	{
		return mono_field_get_value_object(CWMonoRuntime::GetDomain(), m_Field, instance);
	}

	bool CWMonoField::HasAttribute(CWMonoClass* monoClass) const
	{
		MonoClass* parent = mono_field_get_parent(m_Field);
		MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_field(parent, m_Field);
		if (attrInfo == nullptr)
			return false;

		bool hasAttr = mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()) != 0;
		mono_custom_attrs_free(attrInfo);

		return hasAttr;
	}

	MonoPrimitiveType CWMonoField::GetPrimitiveType() const
	{
		MonoType* type = mono_class_get_type(m_Type->GetInternalPtr());
		int primitiveType = mono_type_get_type(type);
		switch(primitiveType)
		{
			case(MONO_TYPE_BOOLEAN):
				return MonoPrimitiveType::Bool;
			case(MONO_TYPE_CHAR):
				return MonoPrimitiveType::Char;
			case(MONO_TYPE_I1):
				return MonoPrimitiveType::I8;
			case(MONO_TYPE_U1):
				return MonoPrimitiveType::U8;
			case(MONO_TYPE_I2):
				return MonoPrimitiveType::I16;
			case(MONO_TYPE_U2):
				return MonoPrimitiveType::U16;
			case(MONO_TYPE_I4):
				return MonoPrimitiveType::I32;
			case(MONO_TYPE_U4):
				return MonoPrimitiveType::U32;
			case(MONO_TYPE_I8):
				return MonoPrimitiveType::I64;
			case(MONO_TYPE_U8):
				return MonoPrimitiveType::U64;
			case(MONO_TYPE_R4):
				return MonoPrimitiveType::R32;
			case(MONO_TYPE_R8):
				return MonoPrimitiveType::R64;
			case(MONO_TYPE_STRING):
				return MonoPrimitiveType::String;
			case(MONO_TYPE_CLASS):
				return MonoPrimitiveType::Class;
			case(MONO_TYPE_VALUETYPE):
				return MonoPrimitiveType::ValueType;
			case(MONO_TYPE_ARRAY):
				return MonoPrimitiveType::Array;
			case(MONO_TYPE_GENERICINST):
				return MonoPrimitiveType::Generic;
			case(MONO_TYPE_ENUM):
				return MonoPrimitiveType::Enum;
			default:
				break;
		}
		
		return MonoPrimitiveType::Unknown;
	}

	MonoObject* CWMonoField::GetAttribute(CWMonoClass* monoClass) const
	{
		MonoClass* parent = mono_field_get_parent(m_Field);
		MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_field(parent, m_Field);
		if (attrInfo == nullptr)
			return nullptr;

		MonoObject* foundAttr = nullptr;
		if (mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()))
			foundAttr = mono_custom_attrs_get_attr(attrInfo, monoClass->GetInternalPtr());

		mono_custom_attrs_free(attrInfo);
		return foundAttr;
	}

	bool CWMonoField::IsValueType() const
	{
		return m_Type->IsValueType();
	}

	bool CWMonoField::IsStatic() const 
	{
		return (mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_STATIC) != 0;
	}

}