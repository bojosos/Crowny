#include "cwpch.h"

#include "Crowny/Scripting/CWMonoField.h"
#include "Crowny/Scripting/CWMonoClass.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>

namespace Crowny
{

	CWMonoField::CWMonoField(MonoClassField* field) : m_Field(field)
	{
		m_Type = new CWMonoType(mono_field_get_type(field));
		m_Name = mono_field_get_name(m_Field);
		m_FullDeclName = CWMonoVisibilityToString(GetVisibility()) + (IsStatic() ? " static " : " ") + mono_field_full_name(m_Field);
		
		if (IsStatic())
		{
			m_OwningTypeVTable = mono_class_vtable(CWMonoRuntime::GetDomain(), mono_type_get_class(mono_field_get_type(field)));
			mono_runtime_class_init(m_OwningTypeVTable);
		}
	}

	CWMonoVisibility CWMonoField::GetVisibility()
	{
		uint32_t flags = mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_FIELD_ACCESS_MASK;
		switch (flags)
		{
			case MONO_FIELD_ATTR_PRIVATE:       return CWMonoVisibility::PRIVATE;
			case MONO_FIELD_ATTR_FAM_AND_ASSEM: return CWMonoVisibility::PROTECTED_INTERNAL;
			case MONO_FIELD_ATTR_ASSEMBLY:      return CWMonoVisibility::INTERNAL;
			case MONO_FIELD_ATTR_FAMILY:        return CWMonoVisibility::PROTECTED;
			case MONO_FIELD_ATTR_PUBLIC:        return CWMonoVisibility::PUBLIC;
		}

		CW_ENGINE_ERROR("Unkown mono type");

		return CWMonoVisibility::PRIVATE;
	}
	
	void CWMonoField::Set(MonoObject* obj, void* value)
	{
		mono_field_set_value (obj, m_Field, value);
	}

	void CWMonoField::Get(MonoObject* obj, void* outval)
	{
		mono_field_get_value(obj, m_Field, outval);
	}

	bool CWMonoField::HasAttribute(CWMonoClass* monoClass)
	{
		MonoClass* parent = mono_field_get_parent(m_Field);
		MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_field(parent, m_Field);
		if (attrInfo == nullptr)
			return false;

		bool hasAttr = mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()) != 0;
		mono_custom_attrs_free(attrInfo);

		return hasAttr;
	}

	MonoObject* CWMonoField::GetAttribute(CWMonoClass* monoClass)
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

	bool CWMonoField::IsValueType()
	{
		return m_Type->IsValueType();
	}

	bool CWMonoField::IsStatic() const 
	{
		return (mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_STATIC) != 0;
	}

}