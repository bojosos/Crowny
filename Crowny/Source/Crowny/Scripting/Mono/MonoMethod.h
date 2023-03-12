#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoVisibility.h"

namespace Crowny
{
    class MonoClass;

    class MonoMethod
    {
    public:
        MonoMethod(::MonoMethod* method);
        const String& GetName() const { return m_Name; };
        const String& GetFullDeclName() const;
        MonoClass* GetParameterType(uint32_t idx) const;
        MonoClass* GetReturnType() const;
        bool IsStatic() const;
        uint32_t GetNumParams() const;
        CrownyMonoVisibility GetVisibility() const;
        bool HasAttribute(MonoClass* monoClass) const;
        MonoObject* GetAttribute(MonoClass* monoClass) const;
        Vector<MonoClass*> GetAttributes() const;
        void* GetThunk() const;
        MonoObject* Invoke(MonoObject* instance, void** params);

    private:
        void CacheSignature() const;

    private:
        ::MonoMethod* m_Method = nullptr;
        String m_Name;

        mutable String m_FullDeclName;
        mutable MonoClass* m_CachedReturnType;
        mutable MonoClass** m_CachedParams;
        mutable uint32_t m_CachedNumParams;
        mutable bool m_IsStatic;
        mutable bool m_HasCachedSignature;
    };
} // namespace Crowny