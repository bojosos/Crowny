#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCollider2DBase : public ComponentBase
    {

    };

    class ScriptPhysics2D : public TScriptComponent<ScriptCollider2DBase, Collider2D,
    {

    }
}