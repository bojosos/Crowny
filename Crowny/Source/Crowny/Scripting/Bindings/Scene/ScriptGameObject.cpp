#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptGameObject.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Scene/SceneManager.h"

namespace Crowny
{

    void ScriptGameObject::InitRuntimeFunctions()
    {
        CWMonoClass* gameObject = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "GameObject");

        gameObject->AddInternalCall("Internal_Find", (void*)&Internal_FindObject);
    }

    MonoObject* ScriptGameObject::Internal_FindObject(MonoString* name)
    {
        Entity e = SceneManager::GetActiveScene()->FindEntityByName(MonoUtils::FromMonoString(name));
        if (e)
            return ScriptComponent::s_EntityComponents[(uint32_t)e];
        return nullptr;
    }
} // namespace Crowny
