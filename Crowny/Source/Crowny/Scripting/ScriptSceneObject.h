#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    // A base class for scene objects. Scene objects are Entities and Components
    class ScriptSceneObjectBase : public PersistentScriptObjectBase
    {
    public:
        ScriptSceneObjectBase(MonoObject* instance);
        virtual ~ScriptSceneObjectBase();

        Entity GetNativeEntity() const { return m_Entity; }
        void SetNativeEntity(Entity entity) { m_Entity = entity; }

        MonoObject* GetManagedInstance() const;

    protected:
        void SetManagedInstance(MonoObject* instance);

        void FreeManagedInstance();

        Entity m_Entity;
        uint32_t m_GCHandle = 0;
    };

} // namespace Crowny