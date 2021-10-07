#pragma once

#include "Crowny/Scripting/CWMonoClass.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

namespace Crowny
{
    class CWMonoAssembly
    {
        struct ClassId
        {
            struct Hash
            {
                size_t operator()(const ClassId& v) const;
            };

            struct Equals
            {
                bool operator()(const ClassId& a, const ClassId& b) const;
            };

            ClassId(const String& namespaceName, const String& name);

            String NamespaceName;
            String Name;
        };

    public:
        CWMonoAssembly(const Path& filepath, const String& name);
        CWMonoAssembly(MonoImage* image, const String& name);
        ~CWMonoAssembly();
        CWMonoClass* GetClass(const String& namespaceName, const String& className) const;
        CWMonoClass* GetClass(const String& fullName) const;
        const Vector<CWMonoClass*>& GetClasses() const;

    private:
        MonoAssembly* m_Assembly;
        MonoImage* m_Image;
        bool m_IsLoaded;

        mutable bool m_AllClassesCached;
        mutable Vector<CWMonoClass*> m_ClassList;
        mutable UnorderedMap<ClassId, CWMonoClass*, ClassId::Hash, ClassId::Equals> m_Classes;
    };
} // namespace Crowny
