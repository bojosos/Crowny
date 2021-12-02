#pragma once

#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{
    class ScriptComponentBase;

    // TODO: Each time a component/entity is destroyed this manager needs to be notified.
    class ScriptSceneObjectManager : public Module<ScriptSceneObjectManager>
    {
    public:
        ScriptSceneObjectManager() = default;
        ~ScriptSceneObjectManager() = default;

        ScriptEntity* GetOrCreateScriptEntity(Entity entity);
        ScriptEntity* CreateScriptEntity(Entity entity);
        ScriptEntity* CreateScriptEntity(MonoObject* existingInstance, Entity entity);
        ScriptEntity* GetScriptEntity(uint32_t id) const;
        ScriptEntity* GetScriptEntity(Entity entity) const;

        ScriptComponentBase* CreateScriptComponent(Entity entity, const ComponentBase& component,
                                                   MonoReflectionType* reflType);
        ScriptEntityBehaviour* CreateScriptComponent(MonoObject* instance, Entity entity,
                                                     const ComponentBase& component);
        ScriptComponentBase* GetScriptComponent(Entity entity, const ComponentBase& component,
                                                MonoReflectionType* reflType, bool create = true);
        ScriptComponentBase* GetScriptComponent(uint64_t instanceId);

        void DestroyScriptEntity(ScriptEntity* entity);
        void DestroyScriptComponent(ScriptComponentBase* scriptComponent, uint64_t instanceId);

    private:
        UnorderedMap<uint64_t, ScriptComponentBase*> m_ScriptComponents;
        UnorderedMap<uint32_t, ScriptEntity*> m_ScriptEntities;
    };

} // namespace Crowny