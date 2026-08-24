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
        static bool Internal_IsTrigger(ScriptCollider3DBase* thisPtr);
        static void Internal_SetTrigger(ScriptCollider3DBase* thisPtr, bool value);
        static void Internal_GetOffset(ScriptCollider3DBase* thisPtr, glm::vec3* value);
        static void Internal_SetOffset(ScriptCollider3DBase* thisPtr, glm::vec3* value);
        static void Internal_GetRotation(ScriptCollider3DBase* thisPtr, glm::quat* value);
        static void Internal_SetRotation(ScriptCollider3DBase* thisPtr, glm::quat* value);
        static MonoObject* Internal_GetMaterial(ScriptCollider3DBase* thisPtr);
        static void Internal_SetMaterial(ScriptCollider3DBase* thisPtr, MonoObject* value);
        static void Internal_GetFilter(ScriptCollider3DBase* thisPtr, PhysicsFilter3D* value);
        static void Internal_SetFilter(ScriptCollider3DBase* thisPtr, PhysicsFilter3D* value);
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
        static void Internal_GetSize(ScriptBoxCollider3D* thisPtr, glm::vec3* value);
        static void Internal_SetSize(ScriptBoxCollider3D* thisPtr, glm::vec3* value);
    };

    class ScriptSphereCollider3D : public TScriptComponent<ScriptSphereCollider3D, SphereCollider3DComponent, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SphereCollider3D")

        ScriptSphereCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
        static float Internal_GetRadius(ScriptSphereCollider3D* thisPtr);
        static void Internal_SetRadius(ScriptSphereCollider3D* thisPtr, float value);
    };

    class ScriptCapsuleCollider3D : public TScriptComponent<ScriptCapsuleCollider3D, CapsuleCollider3DComponent, ScriptCollider3DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "CapsuleCollider3D")

        ScriptCapsuleCollider3D(MonoObject* instance, Entity entity);
        Collider3D& GetCollider3D() override { return GetComponent(); }
        Entity GetEntity() override { return GetNativeEntity(); }

    private:
        static float Internal_GetRadius(ScriptCapsuleCollider3D* thisPtr);
        static void Internal_SetRadius(ScriptCapsuleCollider3D* thisPtr, float value);
        static float Internal_GetHeight(ScriptCapsuleCollider3D* thisPtr);
        static void Internal_SetHeight(ScriptCapsuleCollider3D* thisPtr, float value);
    };
} // namespace Crowny
