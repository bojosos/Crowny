#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class Entity;
    class MonoClass;

    class MonoScriptRuntime
    {
    public:
        bool Bind(MonoObject* instance, MonoClass* scriptClass);
        void Clear();

        MonoObject* GetInstance() const { return m_Instance; }
        MonoClass* GetScriptClass() const { return m_ScriptClass; }
        void Dispatch(Entity self, Entity other, const ScriptEvent& event) const;

    private:
        using LifecycleThunk = void(CW_THUNKCALL*)(MonoObject*, MonoException**);
        using EventThunk = void(CW_THUNKCALL*)(MonoObject*, MonoObject*, MonoException**);

        MonoObject* m_Instance = nullptr;
        MonoClass* m_ScriptClass = nullptr;
        LifecycleThunk m_OnAwake = nullptr;
        LifecycleThunk m_OnStart = nullptr;
        LifecycleThunk m_OnUpdate = nullptr;
        LifecycleThunk m_OnLateUpdate = nullptr;
        LifecycleThunk m_OnFixedUpdate = nullptr;
        LifecycleThunk m_OnDestroy = nullptr;
        EventThunk m_OnCollisionEnter2D = nullptr;
        EventThunk m_OnCollisionStay2D = nullptr;
        EventThunk m_OnCollisionExit2D = nullptr;
        EventThunk m_OnTriggerEnter2D = nullptr;
        EventThunk m_OnTriggerStay2D = nullptr;
        EventThunk m_OnTriggerExit2D = nullptr;
        EventThunk m_OnCollisionEnter3D = nullptr;
        EventThunk m_OnCollisionStay3D = nullptr;
        EventThunk m_OnCollisionExit3D = nullptr;
        EventThunk m_OnTriggerEnter3D = nullptr;
        EventThunk m_OnTriggerStay3D = nullptr;
        EventThunk m_OnTriggerExit3D = nullptr;
    };
} // namespace Crowny
