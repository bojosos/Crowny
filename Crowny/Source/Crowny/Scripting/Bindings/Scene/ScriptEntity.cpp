#include "cwpch.h"

#include "Crowny/Scripting/Backends/Mono/MonoObjectIdentity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"

namespace Crowny
{
    ScriptEntity::ScriptEntity(MonoObject* instance, Entity entity) : ScriptObject(instance)
    {
        m_Entity = entity;
        SetManagedInstance(instance);
        if (!MonoObjectIdentity::SetEntity(instance, entity.GetUuid()))
            CW_ENGINE_ERROR("Could not bind the managed entity identity.");
    }

    void ScriptEntity::InitRuntimeData() {}

    MonoObject* ScriptEntity::CreateManagedInstance(bool construct)
    {
        MonoObject* instance = MetaData.ScriptClass->CreateInstance(construct);
        SetManagedInstance(instance);
        if (!MonoObjectIdentity::SetEntity(instance, m_Entity.GetUuid()))
            CW_ENGINE_ERROR("Could not restore the managed entity identity.");
        return instance;
    }

} // namespace Crowny
