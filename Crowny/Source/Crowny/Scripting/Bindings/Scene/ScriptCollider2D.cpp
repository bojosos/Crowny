#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"

#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <mono/metadata/object.h>

namespace Crowny
{

    ScriptCollider2DBase::ScriptCollider2DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    ScriptCollider2D::ScriptCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider2D::InitRuntimeData()
    {
        // MetaData.ScriptClass->AddInternalCall("Internal")
    }

} // namespace Crowny