#pragma once

#include "Crowny/Physics/Physics2DBackend.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptPhysics2D : public ScriptObject<ScriptPhysics2D>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Physics2D");

    private:
        static Physics2DBackendType Internal_GetBackend();
        static bool Internal_IsSimulating();
        static void Internal_GetGravity(glm::vec2* outGravity);
        static void Internal_SetGravity(glm::vec2* gravity);
        static uint32_t Internal_GetVelocityIterations();
        static void Internal_SetVelocityIterations(uint32_t iterations);
        static uint32_t Internal_GetPositionIterations();
        static void Internal_SetPositionIterations(uint32_t iterations);
        static MonoObject* Internal_GetDefaultMaterial();
        static void Internal_SetDefaultMaterial(MonoObject* material);
        static MonoString* Internal_GetLayerName(int32_t layer);
        static void Internal_SetLayerName(int32_t layer, MonoString* name);
        static uint32_t Internal_GetLayerMask(int32_t layer);
        static void Internal_SetLayerMask(int32_t layer, uint32_t mask);
        static MonoObject* Internal_GetEntity(uint32_t entityId);
        static void Internal_Raycast(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask, MonoArray** outResults);
        static int32_t Internal_RaycastNonAlloc(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask,
                                                MonoArray* results, int32_t capacity);
    };
} // namespace Crowny
