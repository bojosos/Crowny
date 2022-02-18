#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"

#include "Crowny/Scripting/Mono/MonoManager.h"

#include <mono/metadata/attrdefs.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    MonoField::MonoField(MonoClassField* field) : m_Field(field)
    {
        MonoType* type = mono_field_get_type(field);
        ::MonoClass* classType = mono_class_from_mono_type(type);
        if (classType == nullptr)
            m_Type = nullptr;

        m_Type = new MonoClass(classType);
        m_Name = mono_field_get_name(m_Field);
        m_FullDeclName = CrownyMonoVisibilityToString(GetVisibility()) + (IsStatic() ? " static " : " ") +
                         mono_field_full_name(m_Field);
    }

    CrownyMonoVisibility MonoField::GetVisibility() const
    {
        uint32_t flags = mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_FIELD_ACCESS_MASK;
        switch (flags)
        {
        case MONO_FIELD_ATTR_PRIVATE:
            return CrownyMonoVisibility::Private;
        case MONO_FIELD_ATTR_FAM_AND_ASSEM:
            return CrownyMonoVisibility::ProtectedInternal;
        case MONO_FIELD_ATTR_ASSEMBLY:
            return CrownyMonoVisibility::Internal;
        case MONO_FIELD_ATTR_FAMILY:
            return CrownyMonoVisibility::Protected;
        case MONO_FIELD_ATTR_PUBLIC:
            return CrownyMonoVisibility::Public;
        }

        CW_ENGINE_ERROR("Unkown mono type");

        return CrownyMonoVisibility::Private;
    }

    void MonoField::Set(MonoObject* obj, void* value) { mono_field_set_value(obj, m_Field, value); }

    void MonoField::Get(MonoObject* obj, void* outval) { mono_field_get_value(obj, m_Field, outval); }

    MonoObject* MonoField::GetBoxed(MonoObject* instance)
    {
        return mono_field_get_value_object(MonoManager::Get().GetDomain(), m_Field, instance);
    }

    bool MonoField::HasAttribute(MonoClass* monoClass) const
    {
        ::MonoClass* parent = mono_field_get_parent(m_Field);
        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_field(parent, m_Field);
        if (attrInfo == nullptr)
            return false;

        bool hasAttr = mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(attrInfo);

        return hasAttr;
    }

    MonoObject* MonoField::GetAttribute(MonoClass* monoClass) const
    {
        ::MonoClass* parent = mono_field_get_parent(m_Field);
        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_field(parent, m_Field);
        if (attrInfo == nullptr)
            return nullptr;

        MonoObject* foundAttr = nullptr;
        if (mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()))
            foundAttr = mono_custom_attrs_get_attr(attrInfo, monoClass->GetInternalPtr());

        mono_custom_attrs_free(attrInfo);
        return foundAttr;
    }

    bool MonoField::IsValueType() const { return m_Type->IsValueType(); }

    bool MonoField::IsStatic() const { return (mono_field_get_flags(m_Field) & MONO_FIELD_ATTR_STATIC) != 0; }

} // namespace Crowny