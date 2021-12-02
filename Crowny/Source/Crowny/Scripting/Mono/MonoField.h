#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/Mono/MonoVisibility.h"

namespace Crowny
{
    class MonoClass;

    class MonoField
    {
    public:
        MonoField(MonoClassField* field);
        const String& GetName() const { return m_Name; };
        const String& GetFullDeclName() const { return m_FullDeclName; }
        MonoClass* GetType() const { return m_Type; };
        CrownyMonoVisibility GetVisibility() const;
        bool IsStatic() const;
        MonoPrimitiveType GetPrimitiveType() const;

        bool IsValueType() const;
        bool HasAttribute(MonoClass* monoClass) const;
        MonoObject* GetAttribute(MonoClass* monoClass) const;
        void Set(MonoObject* obj, void* value);
        MonoObject* GetBoxed(MonoObject* instance);
        void Get(MonoObject* obj, void* outval);

    private:
        MonoClassField* m_Field = nullptr;
        MonoClass* m_Type;
        String m_Name;
        String m_FullDeclName;
    };
} // namespace Crowny