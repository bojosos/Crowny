#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptPhysics3D : public ScriptObject<ScriptPhysics3D>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Physics3D")

    private:
        static int32_t Internal_GetBackend();
        static MonoString* Internal_GetBackendName();
        static bool Internal_IsSimulating();
        static uint64_t Internal_GetCapabilities();
        static uint32_t Internal_GetSubsteps();
        static void Internal_SetSubsteps(uint32_t substeps);
        static MonoObject* Internal_GetDefaultMaterial();
        static void Internal_SetDefaultMaterial(MonoObject* material);
        static void Internal_GetGravity(glm::vec3* outGravity);
        static void Internal_SetGravity(glm::vec3* gravity);
        static bool Internal_TrySetBackend(int32_t value);
        static bool Internal_IsBackendAvailable(int32_t value);
        static MonoObject* Internal_GetEntity(uint64_t entityId);
        static void Internal_Raycast(glm::vec3* origin, glm::vec3* direction, float distance, uint32_t layerMask, bool includeTriggers,
                                     uint64_t ignoreBodyHandle, MonoArray** outResults);
        static int32_t Internal_RaycastNonAlloc(glm::vec3* origin, glm::vec3* direction, float distance, uint32_t layerMask,
                                                bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray* results, int32_t capacity);
        static void Internal_Sweep(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                   glm::quat* rotation, glm::vec3* direction, float distance, uint32_t layerMask,
                                   bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray** outResults);
        static int32_t Internal_SweepNonAlloc(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                              glm::quat* rotation, glm::vec3* direction, float distance, uint32_t layerMask,
                                              bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray* results, int32_t capacity);
        static void Internal_Overlap(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                     glm::quat* rotation, uint32_t layerMask, bool includeTriggers, uint64_t ignoreBodyHandle,
                                     MonoArray** outResults);
        static int32_t Internal_OverlapNonAlloc(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                                glm::quat* rotation, uint32_t layerMask, bool includeTriggers, uint64_t ignoreBodyHandle,
                                                MonoArray* results, int32_t capacity);
    };
} // namespace Crowny
