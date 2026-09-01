#include "cwpch.h"

#include <mono/metadata/object.h>

#include "Crowny/Scripting/Backends/Mono/MonoScriptRuntime.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{
    namespace
    {
        struct Collision2DInterop
        {
            MonoArray* Colliders = nullptr;
            MonoArray* ContactPoints = nullptr;
        };

        struct Collision3DInterop
        {
            MonoArray* Colliders = nullptr;
            MonoArray* Contacts = nullptr;
        };

        struct ContactPoint3DInterop
        {
            glm::vec3 Point;
            glm::vec3 Normal;
            float Separation;
            float NormalImpulse;
        };

        MonoObject* GetManagedEntity(Entity entity)
        {
            if (!entity)
                return nullptr;
            ScriptEntity* managed = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity);
            return managed != nullptr ? managed->GetManagedInstance() : nullptr;
        }

        MonoObject* CreateCollision2D(Entity self, Entity other, const ScriptEvent& event)
        {
            Collision2DInterop data;
            data.Colliders = mono_array_new(MonoManager::Get().GetDomain(), ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(), 2);
            if (MonoObject* managedSelf = GetManagedEntity(self))
                mono_array_setref(data.Colliders, 0, managedSelf);
            if (MonoObject* managedOther = GetManagedEntity(other))
                mono_array_setref(data.Colliders, 1, managedOther);

            MonoClass* vectorClass = MonoManager::Get().FindClass("Crowny", "Vector2");
            MonoClass* collisionClass = MonoManager::Get().FindClass("Crowny", "Collision2D");
            if (vectorClass == nullptr || collisionClass == nullptr)
                return nullptr;
            data.ContactPoints = mono_array_new(MonoManager::Get().GetDomain(), vectorClass->GetInternalPtr(), event.Contacts.size());
            for (size_t index = 0; index < event.Contacts.size(); ++index)
            {
                const glm::vec2 point(event.Contacts[index].Position.x, event.Contacts[index].Position.y);
                mono_array_set(data.ContactPoints, glm::vec2, index, point);
            }
            return MonoUtils::Box(collisionClass->GetInternalPtr(), &data);
        }

        MonoObject* CreateCollision3D(Entity self, Entity other, const ScriptEvent& event)
        {
            Collision3DInterop data;
            data.Colliders = mono_array_new(MonoManager::Get().GetDomain(), ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(), 2);
            if (MonoObject* managedSelf = GetManagedEntity(self))
                mono_array_setref(data.Colliders, 0, managedSelf);
            if (MonoObject* managedOther = GetManagedEntity(other))
                mono_array_setref(data.Colliders, 1, managedOther);

            MonoClass* contactClass = MonoManager::Get().FindClass("Crowny", "ContactPoint3D");
            MonoClass* collisionClass = MonoManager::Get().FindClass("Crowny", "Collision3D");
            if (contactClass == nullptr || collisionClass == nullptr)
                return nullptr;
            data.Contacts = mono_array_new(MonoManager::Get().GetDomain(), contactClass->GetInternalPtr(), event.Contacts.size());
            for (size_t index = 0; index < event.Contacts.size(); ++index)
            {
                const ScriptContactPoint& point = event.Contacts[index];
                const ContactPoint3DInterop managedPoint{ point.Position, point.Normal, point.Separation, point.Impulse };
                mono_array_set(data.Contacts, ContactPoint3DInterop, index, managedPoint);
            }
            return MonoUtils::Box(collisionClass->GetInternalPtr(), &data);
        }

        template <typename Thunk> void Invoke(Thunk thunk, MonoObject* instance, MonoObject* argument)
        {
            if (thunk != nullptr && instance != nullptr && argument != nullptr)
                MonoUtils::InvokeThunk(thunk, instance, argument);
        }
    } // namespace

    bool MonoScriptRuntime::Bind(MonoObject* instance, MonoClass* scriptClass)
    {
        Clear();
        if (instance == nullptr || scriptClass == nullptr)
            return false;
        m_Instance = instance;
        m_ScriptClass = scriptClass;

        for (MonoClass* current = scriptClass; current != nullptr; current = current->GetBaseClass())
        {
            const auto bindLifecycle = [&](LifecycleThunk& thunk, const char* name) {
                if (thunk == nullptr)
                    if (MonoMethod* method = current->GetMethod(name, 0))
                        thunk = reinterpret_cast<LifecycleThunk>(method->GetThunk());
            };
            const auto bindEvent = [&](EventThunk& thunk, const char* name, const char* parameter) {
                if (thunk == nullptr)
                    if (MonoMethod* method = current->GetMethod(name, parameter))
                        thunk = reinterpret_cast<EventThunk>(method->GetThunk());
            };
            const auto bindEventAlias = [&](EventThunk& thunk, const char* name, const char* alias, const char* parameter) {
                if (thunk != nullptr)
                    return;
                MonoMethod* method = current->GetMethod(name, parameter);
                if (method == nullptr)
                    method = current->GetMethod(alias, parameter);
                if (method != nullptr)
                    thunk = reinterpret_cast<EventThunk>(method->GetThunk());
            };

            bindLifecycle(m_OnStart, "Start");
            bindLifecycle(m_OnUpdate, "Update");
            bindLifecycle(m_OnFixedUpdate, "FixedUpdate");
            bindLifecycle(m_OnDestroy, "Destroy");
            bindEvent(m_OnCollisionEnter2D, "OnCollisionEnter2D", "Collision2D");
            bindEvent(m_OnCollisionStay2D, "OnCollisionStay2D", "Collision2D");
            bindEvent(m_OnCollisionExit2D, "OnCollisionExit2D", "Collision2D");
            bindEvent(m_OnTriggerEnter2D, "OnTriggerEnter2D", "Entity");
            bindEvent(m_OnTriggerStay2D, "OnTriggerStay2D", "Entity");
            bindEvent(m_OnTriggerExit2D, "OnTriggerExit2D", "Entity");
            bindEventAlias(m_OnCollisionEnter3D, "OnCollisionEnter3D", "OnCollisionEnter", "Collision3D");
            bindEventAlias(m_OnCollisionStay3D, "OnCollisionStay3D", "OnCollisionStay", "Collision3D");
            bindEventAlias(m_OnCollisionExit3D, "OnCollisionExit3D", "OnCollisionExit", "Collision3D");
            bindEventAlias(m_OnTriggerEnter3D, "OnTriggerEnter3D", "OnTriggerEnter", "Entity");
            bindEventAlias(m_OnTriggerStay3D, "OnTriggerStay3D", "OnTriggerStay", "Entity");
            bindEventAlias(m_OnTriggerExit3D, "OnTriggerExit3D", "OnTriggerExit", "Entity");

            if (current->GetBaseClass() == ScriptEntityBehaviour::GetMetaData()->ScriptClass)
                break;
        }
        return true;
    }

    void MonoScriptRuntime::Clear()
    {
        *this = {};
    }

    void MonoScriptRuntime::Dispatch(Entity self, Entity other, const ScriptEvent& event) const
    {
        switch (event.Kind)
        {
        case ScriptEventKind::Start:
            if (m_OnStart != nullptr)
                MonoUtils::InvokeThunk(m_OnStart, m_Instance);
            break;
        case ScriptEventKind::Update:
            if (m_OnUpdate != nullptr)
                MonoUtils::InvokeThunk(m_OnUpdate, m_Instance);
            break;
        case ScriptEventKind::FixedUpdate:
            if (m_OnFixedUpdate != nullptr)
                MonoUtils::InvokeThunk(m_OnFixedUpdate, m_Instance);
            break;
        case ScriptEventKind::Destroy:
            if (m_OnDestroy != nullptr)
                MonoUtils::InvokeThunk(m_OnDestroy, m_Instance);
            break;
        case ScriptEventKind::CollisionEnter2D:
            Invoke(m_OnCollisionEnter2D, m_Instance, CreateCollision2D(self, other, event));
            break;
        case ScriptEventKind::CollisionStay2D:
            Invoke(m_OnCollisionStay2D, m_Instance, CreateCollision2D(self, other, event));
            break;
        case ScriptEventKind::CollisionExit2D:
            Invoke(m_OnCollisionExit2D, m_Instance, CreateCollision2D(self, other, event));
            break;
        case ScriptEventKind::TriggerEnter2D:
            Invoke(m_OnTriggerEnter2D, m_Instance, GetManagedEntity(other));
            break;
        case ScriptEventKind::TriggerStay2D:
            Invoke(m_OnTriggerStay2D, m_Instance, GetManagedEntity(other));
            break;
        case ScriptEventKind::TriggerExit2D:
            Invoke(m_OnTriggerExit2D, m_Instance, GetManagedEntity(other));
            break;
        case ScriptEventKind::CollisionEnter3D:
            Invoke(m_OnCollisionEnter3D, m_Instance, CreateCollision3D(self, other, event));
            break;
        case ScriptEventKind::CollisionStay3D:
            Invoke(m_OnCollisionStay3D, m_Instance, CreateCollision3D(self, other, event));
            break;
        case ScriptEventKind::CollisionExit3D:
            Invoke(m_OnCollisionExit3D, m_Instance, CreateCollision3D(self, other, event));
            break;
        case ScriptEventKind::TriggerEnter3D:
            Invoke(m_OnTriggerEnter3D, m_Instance, GetManagedEntity(other));
            break;
        case ScriptEventKind::TriggerStay3D:
            Invoke(m_OnTriggerStay3D, m_Instance, GetManagedEntity(other));
            break;
        case ScriptEventKind::TriggerExit3D:
            Invoke(m_OnTriggerExit3D, m_Instance, GetManagedEntity(other));
            break;
        }
    }
} // namespace Crowny
