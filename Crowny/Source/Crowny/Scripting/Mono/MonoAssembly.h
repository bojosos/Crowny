#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoClass.h"

namespace Crowny
{
    class MonoAssembly
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
        ~MonoAssembly();

        MonoClass* GetClass(const String& namespaceName, const String& className) const;
        MonoClass* GetClass(const String& fullName) const;
        MonoClass* GetClass(::MonoClass* rawClass) const;

        const String& GetName() const { return m_Name; }

        const Vector<MonoClass*>& GetClasses() const;

        void Load();
        void LoadFromImage(MonoImage* image);
        void Unload();

    private:
        friend class MonoManager;

        MonoAssembly(const Path& filepath, const String& name);

        ::MonoAssembly* m_Assembly;
        MonoImage* m_Image;
        bool m_IsLoaded;
        bool m_IsDependency;
        uint8_t* m_DebugData;
        String m_Name;
        Path m_Path;

        mutable bool m_AllClassesCached;
        mutable Vector<MonoClass*> m_ClassList;
        mutable UnorderedMap<ClassId, MonoClass*, ClassId::Hash, ClassId::Equals> m_Classes;
        mutable UnorderedMap<::MonoClass*, MonoClass*> m_ClassesByRaw;
    };
} // namespace Crowny
