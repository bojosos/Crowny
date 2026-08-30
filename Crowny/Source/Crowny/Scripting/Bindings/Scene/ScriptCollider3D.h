#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCollider3DBase : public ScriptComponentBase
    {
    public:
        ScriptCollider3DBase(MonoObject* instance);
        virtual ~ScriptCollider3DBase() = default;

        virtual Collider3D& GetCollider3D() = 0;
        virtual Entity GetEntity() = 0;

    protected:
    };

    class ScriptCollider3D : public TScriptComponent<ScriptCollider3D, Collider3D, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Collider3D")

        ScriptCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }
    };

    class ScriptBoxCollider3D : public TScriptComponent<ScriptBoxCollider3D, BoxCollider3DComponent, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "BoxCollider3D")

        ScriptBoxCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
    };

    class ScriptSphereCollider3D : public TScriptComponent<ScriptSphereCollider3D, SphereCollider3DComponent, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SphereCollider3D")

        ScriptSphereCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
    };

    class ScriptCapsuleCollider3D : public TScriptComponent<ScriptCapsuleCollider3D, CapsuleCollider3DComponent, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "CapsuleCollider3D")

        ScriptCapsuleCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
    };
} // namespace Crowny
