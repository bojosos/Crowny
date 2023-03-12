#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCollider2DBase : public ScriptComponentBase
    {
    public:
        ScriptCollider2DBase(MonoObject* instance);
        virtual ~ScriptCollider2DBase() {}
    };

    class ScriptCollider2D : public TScriptComponent<ScriptCollider2D, Collider2D, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Collider2D");

        ScriptCollider2D(MonoObject* instance, Entity entity);
    };

    class ScriptBoxCollider2D
      : public TScriptComponent<ScriptBoxCollider2D, BoxCollider2DComponent, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "BoxCollider2D");
        ScriptBoxCollider2D(MonoObject* instance, Entity entity);

    private:
        static void Internal_GetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* offset);
        static void Internal_SetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* offset);

        static bool Internal_IsTrigger(ScriptBoxCollider2D* thisPtr);
        static void Internal_SetTrigger(ScriptBoxCollider2D* thisPtr, bool trigger);
        static void Internal_GetOffset(ScriptBoxCollider2D* thisPtr, glm::vec2* offset);
        static void Internal_SetOffset(ScriptBoxCollider2D* thisPtr, glm::vec2* offset);
    };

    class ScriptCircleCollider2D
      : public TScriptComponent<ScriptCircleCollider2D, CircleCollider2DComponent, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "CircleCollider2D");

        ScriptCircleCollider2D(MonoObject* instance, Entity entity);

    private:
        static float Internal_GetRadius(ScriptCircleCollider2D* thisPtr);
        static void Internal_SetRadius(ScriptCircleCollider2D* thisPtr, float radius);

        static bool Internal_IsTrigger(ScriptCircleCollider2D* thisPtr);
        static void Internal_SetTrigger(ScriptCircleCollider2D* thisPtr, bool trigger);
        static void Internal_GetOffset(ScriptCircleCollider2D* thisPtr, glm::vec2* offset);
        static void Internal_SetOffset(ScriptCircleCollider2D* thisPtr, glm::vec2* offset);
    };
} // namespace Crowny