#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoVisibility.h"

namespace Crowny
{

    class MonoClass;

    class MonoProperty
    {
    public:
        MonoProperty(::MonoProperty* property);

        const String& GetName() const { return m_Name; }
        MonoClass* GetReturnType() const;
        bool HasAttribute(MonoClass* monoClass) const;
        MonoObject* GetAttribute(MonoClass* monoClass) const;
        Vector<MonoClass*> GetAttributes() const;
        bool IsIndexed() const;
        MonoObject* GetIndexed(MonoObject* instance, uint32_t index) const;
        void SetIndexed(MonoObject* instance, uint32_t index, void* value) const;
        void Set(MonoObject* instance, void* value) const;
        MonoObject* Get(MonoObject* instance) const;
        CrownyMonoVisibility GetVisibility() const;

    private:
        void InitializeDeferred() const;

    private:
        String m_Name;
        ::MonoProperty* m_Property;
        ::MonoMethod* m_GetMethod;
        ::MonoMethod* m_SetMethod;

        mutable MonoClass* m_ReturnType;
        mutable bool m_IsIndexed;
        mutable bool m_IsFullyInitialized;
    };

} // namespace Crowny