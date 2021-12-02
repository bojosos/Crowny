#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include "Crowny/Scene/SceneManager.h"

namespace Crowny
{

    ScriptEntity::ScriptEntity(MonoObject* instance, Entity entity) : ScriptObject(instance)
    {
        m_Entity = entity;
        SetManagedInstance(instance);
    }

    void ScriptEntity::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetName", (void*)&Internal_GetName);
        MetaData.ScriptClass->AddInternalCall("Internal_SetName", (void*)&Internal_SetName);
        MetaData.ScriptClass->AddInternalCall("Internal_GetParent", (void*)&Internal_GetParent);
        MetaData.ScriptClass->AddInternalCall("Internal_FindByName", (void*)&Internal_FindEntityByName);
    }

    MonoString* ScriptEntity::Internal_GetName(ScriptEntity* thisptr) { return nullptr; }

    void ScriptEntity::Internal_SetName(ScriptEntity* thisptr, MonoString* string) {}

    MonoObject* ScriptEntity::Internal_GetParent(ScriptEntity* thisptr) { return nullptr; }

    MonoObject* ScriptEntity::Internal_FindEntityByName(MonoString* name)
    {
        Entity e = SceneManager::GetActiveScene()->FindEntityByName(MonoUtils::FromMonoString(name));
        if (e)
            return ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(e)->GetManagedInstance();
        return nullptr;
    }

} // namespace Crowny