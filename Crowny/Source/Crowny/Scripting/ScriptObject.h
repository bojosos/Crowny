#pragma once

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptMeta.h"

namespace Crowny
{

    struct ScriptObjectBackupData
    {
        uint8_t* Data = nullptr;
        uint32_t Size = 0;
    };

    class ScriptObjectBase
    {
    public:
        ScriptObjectBase(MonoObject* instance);
        virtual ~ScriptObjectBase();

        virtual bool IsPersistent() const { return false; }

        virtual void ClearManagedInstance() {}
        virtual void RestoreManagedInstance() {}

        virtual void OnManagedInstanceDeleted(bool assemblyRefresh);
        virtual ScriptObjectBackupData BeginRefresh();
        virtual void EndRefresh(const ScriptObjectBackupData& data);
    };

    class PersistentScriptObjectBase : public ScriptObjectBase
    {
    public:
        PersistentScriptObjectBase(MonoObject* instance);
        virtual ~PersistentScriptObjectBase() = default;

        virtual bool IsPersistent() const override { return true; }
    };

    template <typename Type, class Base> class ScriptObject;

    template <class Type, class Base> struct InitScriptObjectOnStart
    {
    public:
        InitScriptObjectOnStart() { ScriptObject<Type, Base>::InitMetaData(); }

        void MakeSureInstantiated() {}
    };

    template <typename Type, class Base = ScriptObjectBase> class ScriptObject : public Base
    {
    public:
        ScriptObject() : Base(nullptr) { s_InitOnStart.MakeSureInstantiated(); }

        ScriptObject(MonoObject* instance) : Base(instance)
        {
            s_InitOnStart.MakeSureInstantiated();
            Type* param = (Type*)(Base*)this;
            if (MetaData.CachedPtrField != nullptr)
                MetaData.CachedPtrField->Set(instance, &param);
        }

        virtual ~ScriptObject() = default;

        void RestoreManagedInstance()
        {
            MonoObject* instance = CreateManagedInstance(true);
            Type* param = (Type*)(Base*)this;
            if (MetaData.CachedPtrField != nullptr && instance != nullptr)
                MetaData.CachedPtrField->Set(instance, &param);
        }

        virtual MonoObject* CreateManagedInstance(bool construct)
        {
            return MetaData.ScriptClass->CreateInstance(construct);
        }

        static Type* ToNative(MonoObject* managedInstance)
        {
            Type* nativeInstance = nullptr;
            if (MetaData.CachedPtrField != nullptr && managedInstance != nullptr)
                MetaData.CachedPtrField->Get(managedInstance, &nativeInstance);

            return nativeInstance;
        }

        static const ScriptMeta* GetMetaData() { return &MetaData; }

        static void InitMetaData()
        {
            ScriptMeta localMetaData =
              ScriptMeta(Type::GetAssemblyName(), Type::GetNamespace(), Type::GetTypeName(), &Type::InitRuntimeData);
            MonoManager::RegisterScriptType(&MetaData, localMetaData);
        }

    protected:
        static ScriptMeta MetaData;

    private:
        static InitScriptObjectOnStart<Type, Base> s_InitOnStart;
    };

    template <typename Type, typename Base> InitScriptObjectOnStart<Type, Base> ScriptObject<Type, Base>::s_InitOnStart;

    template <typename Type, typename Base> ScriptMeta ScriptObject<Type, Base>::MetaData;

#define SCRIPT_WRAPPER(assembly, ns, name)                                                                             \
    static String GetAssemblyName() { return assembly; }                                                               \
    static String GetNamespace() { return ns; }                                                                        \
    static String GetTypeName() { return name; }                                                                       \
    static void InitRuntimeData();

    class ScriptObjectWrapper : public ScriptObject<ScriptObjectWrapper>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "ScriptOjbect");

    private:
        ScriptObjectWrapper(MonoObject* instance);

        static void Internal_ManagedInstanceDeleted(ScriptObjectBase* instance);
    };
} // namespace Crowny