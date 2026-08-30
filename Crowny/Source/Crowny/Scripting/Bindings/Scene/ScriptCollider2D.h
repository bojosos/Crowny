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

        virtual Collider2D& GetCollider2D() = 0;
        virtual Entity GetEntity() = 0;

    protected:
        // Shared internal calls registered on the base Collider2D C# class
    };

    class ScriptCollider2D : public TScriptComponent<ScriptCollider2D, Collider2D, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Collider2D");

        ScriptCollider2D(MonoObject* instance, Entity entity);

        Collider2D& GetCollider2D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }
    };

    class ScriptBoxCollider2D : public TScriptComponent<ScriptBoxCollider2D, BoxCollider2DComponent, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "BoxCollider2D");
        ScriptBoxCollider2D(MonoObject* instance, Entity entity);

        Collider2D& GetCollider2D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
    };

    class ScriptCircleCollider2D : public TScriptComponent<ScriptCircleCollider2D, CircleCollider2DComponent, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "CircleCollider2D");

        ScriptCircleCollider2D(MonoObject* instance, Entity entity);

        Collider2D& GetCollider2D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
    };
} // namespace Crowny
