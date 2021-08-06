#pragma once

#include "Crowny/Scripting/CWMono.h"
#include "Crowny/Scripting/CWMonoVisibility.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
END_MONO_INCLUDE

namespace Crowny
{

    class CWMonoClass;

    class CWMonoProperty
    {
    public:
        CWMonoProperty(MonoProperty* property);

        const std::string& GetName() const { return m_Name; }
        MonoObject* GetAttribute(CWMonoClass* monoClass) const;
        CWMonoClass* GetReturnType() const;
        bool HasAttribute(CWMonoClass* monoClass) const;
        bool IsIndexed() const;
        MonoObject* GetIndexed(MonoObject* instance, uint32_t index) const;
        void SetIndexed(MonoObject* instance, uint32_t index, void* value) const;
        void Set(MonoObject* instance, void* value) const;
        MonoObject* Get(MonoObject* instance) const;
        CWMonoVisibility GetVisibility() const;

    private:
        void InitializeDeferred() const;

    private:
        std::string m_Name;
        MonoProperty* m_Property;
        MonoMethod* m_GetMethod;
        MonoMethod* m_SetMethod;

        mutable CWMonoClass* m_ReturnType;
        mutable bool m_IsIndexed;
        mutable bool m_IsFullyInitialized;
    };

} // namespace Crowny