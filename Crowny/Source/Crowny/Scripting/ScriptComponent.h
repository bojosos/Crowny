#pragma once

#include "Crowny/Scripting/ScriptObject.h"
#include "Crowny/Scripting/ScriptSceneObject.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{

    class ScriptComponentBase : public ScriptSceneObjectBase
    {
    public:
        ScriptComponentBase(MonoObject* instance);
        virtual ~ScriptComponentBase() = default;
        virtual void NotifyDestroyed() {}
    };
    // The base class of all components
    // Components are stored as the entities they belong to as that better fits entt (Components will frequently change
    // location in memory)
    template <class ScriptClass, class CompType, class BaseType = ScriptComponentBase>
    class TScriptComponent : public ScriptObject<ScriptClass, BaseType>
    {
    public:
        ComponentBase* GetNativeComponent() const { return &this->GetNativeEntity().template GetComponent<CompType>(); }
        void SetNativeComponent() const
        { /* return m_Entity.GetComponent<CompType>();*/
        } // TODO: Replace component
    protected:
        friend class ScriptGameObjectManager;

        TScriptComponent(MonoObject* instance, Entity entity) : ScriptObject<ScriptClass, BaseType>(instance)
        {
            this->SetManagedInstance(instance);
            this->SetNativeEntity(entity);
        }

        virtual ~TScriptComponent() = default;

        virtual MonoObject* CreateManagedInstance(bool construct) override
        {
            MonoObject* managedInstance = ScriptClass::MetaData.ScriptClass->CreateInstance(construct);
            BaseType::SetManagedInstance(managedInstance);
            return managedInstance;
        }

        virtual void NotifyDestroyed() override { this->FreeManagedInstance(); }

        virtual void ClearManagedInstance() override { this->FreeManagedInstance(); }

        CompType& GetComponent() { return this->GetNativeEntity().template GetComponent<CompType>(); }

        virtual void OnManagedInstanceDeleted(bool assemblyRefresh) override
        {
            this->FreeManagedInstance();
            if (!assemblyRefresh && this->GetNativeEntity().template HasComponent<CompType>())
                ScriptSceneObjectManager::Get().DestroyScriptComponent(this, GetComponent().InstanceId);
        }
    };

    class ScriptComponent : public ScriptObject<ScriptComponent, ScriptComponentBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Component");

    private:
        ScriptComponent(MonoObject* instance);

        static MonoObject* Internal_AddComponent(MonoObject* parentEntity, MonoReflectionType* type);
        static bool Internal_HasComponent(MonoObject* parentEntity, MonoReflectionType* type);
        static MonoObject* Internal_GetComponent(MonoObject* parentEntity, MonoReflectionType* type);
        static void Internal_RemoveComponent(MonoObject* parentEntity, MonoReflectionType* type);
        static MonoObject* Internal_GetEntity(ScriptComponentBase* nativeInstance);
    };

} // namespace Crowny