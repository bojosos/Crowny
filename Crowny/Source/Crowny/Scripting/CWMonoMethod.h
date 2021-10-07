#pragma once

#include "Crowny/Scripting/CWMono.h"
#include "Crowny/Scripting/CWMonoVisibility.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{
    class CWMonoClass;

    class CWMonoMethod
    {
    public:
        CWMonoMethod(MonoMethod* method);
        const String& GetName() const { return m_Name; };
        const String& GetFullDeclName() const { return m_FullDeclName; }
        Vector<CWMonoClass*> GetParameterTypes();
        CWMonoClass* GetReturnType();
        bool IsStatic();
        bool IsVirtual();
        CWMonoVisibility GetVisibility();
        bool HasAttribute(CWMonoClass* monoClass) const;
        MonoObject* GetAttribute(CWMonoClass* monoClass) const;
        void* GetThunk() const;
        MonoObject* Invoke(MonoObject* instance, void** params);

    private:
        MonoMethod* m_Method = nullptr;
        MonoMethodSignature* m_Signature = nullptr;
        String m_Name;
        String m_FullDeclName;
    };
} // namespace Crowny