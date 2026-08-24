#include "cwpch.h"

#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics3D.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

#include <mono/metadata/object.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Crowny
{
    namespace
    {
        struct RaycastHit3DInterop
        {
            float PointX;
            float PointY;
            float PointZ;
            float NormalX;
            float NormalY;
            float NormalZ;
            float Distance;
            float Fraction;
            uint64_t BodyHandle;
            uint64_t ShapeHandle;
            uint64_t EntityId;
        };

        bool IsValidBackend(int32_t value)
        {
            return value >= static_cast<int32_t>(Physics3DBackendType::Box3D) && value <= static_cast<int32_t>(Physics3DBackendType::Bullet);
        }

        bool MakeQueryShape(int32_t shapeType, const glm::vec3& size, float radius, float height, PhysicsShape3DDesc& outShape)
        {
            constexpr float minimumExtent = 0.0001f;
            switch (shapeType)
            {
            case static_cast<int32_t>(PhysicsShapeType3D::Box):
                outShape.Type = PhysicsShapeType3D::Box;
                outShape.HalfExtents = glm::max(glm::abs(size) * 0.5f, glm::vec3(minimumExtent));
                return true;
            case static_cast<int32_t>(PhysicsShapeType3D::Sphere):
                outShape.Type = PhysicsShapeType3D::Sphere;
                outShape.Radius = std::max(std::abs(radius), minimumExtent);
                return true;
            case static_cast<int32_t>(PhysicsShapeType3D::Capsule):
                outShape.Type = PhysicsShapeType3D::Capsule;
                outShape.Radius = std::max(std::abs(radius), minimumExtent);
                outShape.Height = std::max(std::abs(height), minimumExtent);
                return true;
            default:
                return false;
            }
        }

        RaycastHit3DInterop ConvertQueryHit(const PhysicsQueryHit3D& hit)
        {
            return RaycastHit3DInterop{ hit.Point.x,  hit.Point.y,  hit.Point.z,     hit.Normal.x,
                                        hit.Normal.y, hit.Normal.z, hit.Distance,    hit.Fraction,
                                        hit.Body.Value, hit.Shape.Value, hit.UserData };
        }

        int32_t WriteQueryResults(const Vector<PhysicsQueryHit3D>& hits, MonoArray* results, int32_t capacity)
        {
            if (!results || capacity <= 0)
                return 0;
            const size_t count = std::min({ hits.size(), static_cast<size_t>(capacity), static_cast<size_t>(mono_array_length(results)) });
            for (size_t i = 0; i < count; ++i)
            {
                const RaycastHit3DInterop converted = ConvertQueryHit(hits[i]);
                mono_array_set(results, RaycastHit3DInterop, i, converted);
            }
            return static_cast<int32_t>(count);
        }

        void SetQueryResults(const Vector<PhysicsQueryHit3D>& hits, MonoArray** outResults)
        {
            if (!outResults)
                return;
            *outResults = nullptr;
            if (hits.empty())
                return;

            MonoAssembly* const assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
            MonoClass* const hitClass = assembly ? assembly->GetClass(CROWNY_NS, "RaycastHit3D") : nullptr;
            if (!hitClass)
            {
                CW_ENGINE_ERROR("Cannot find {0}.RaycastHit3D class.", CROWNY_NS);
                return;
            }

            *outResults = mono_array_new(MonoManager::Get().GetDomain(), hitClass->GetInternalPtr(), static_cast<uintptr_t>(hits.size()));
            for (size_t i = 0; i < hits.size(); ++i)
            {
                const RaycastHit3DInterop converted = ConvertQueryHit(hits[i]);
                mono_array_set(*outResults, RaycastHit3DInterop, i, converted);
            }
        }
    } // namespace

    void ScriptPhysics3D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetBackend", (void*)&Internal_GetBackend);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBackendName", (void*)&Internal_GetBackendName);
        MetaData.ScriptClass->AddInternalCall("Internal_IsSimulating", (void*)&Internal_IsSimulating);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCapabilities", (void*)&Internal_GetCapabilities);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSubsteps", (void*)&Internal_GetSubsteps);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSubsteps", (void*)&Internal_SetSubsteps);
        MetaData.ScriptClass->AddInternalCall("Internal_GetDefaultMaterial", (void*)&Internal_GetDefaultMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_SetDefaultMaterial", (void*)&Internal_SetDefaultMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_GetGravity", (void*)&Internal_GetGravity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetGravity", (void*)&Internal_SetGravity);
        MetaData.ScriptClass->AddInternalCall("Internal_TrySetBackend", (void*)&Internal_TrySetBackend);
        MetaData.ScriptClass->AddInternalCall("Internal_IsBackendAvailable", (void*)&Internal_IsBackendAvailable);
        MetaData.ScriptClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
        MetaData.ScriptClass->AddInternalCall("Internal_Raycast", (void*)&Internal_Raycast);
        MetaData.ScriptClass->AddInternalCall("Internal_RaycastNonAlloc", (void*)&Internal_RaycastNonAlloc);
        MetaData.ScriptClass->AddInternalCall("Internal_Sweep", (void*)&Internal_Sweep);
        MetaData.ScriptClass->AddInternalCall("Internal_SweepNonAlloc", (void*)&Internal_SweepNonAlloc);
        MetaData.ScriptClass->AddInternalCall("Internal_Overlap", (void*)&Internal_Overlap);
        MetaData.ScriptClass->AddInternalCall("Internal_OverlapNonAlloc", (void*)&Internal_OverlapNonAlloc);
    }

    int32_t ScriptPhysics3D::Internal_GetBackend() { return Physics3D::IsStartedUp() ? static_cast<int32_t>(Physics3D::Get().GetBackend()) : 0; }

    MonoString* ScriptPhysics3D::Internal_GetBackendName()
    {
        const String backendName = Physics3D::IsStartedUp() ? String(Physics3D::Get().GetBackendName()) : String();
        return MonoUtils::ToMonoString(backendName);
    }

    bool ScriptPhysics3D::Internal_IsSimulating() { return Physics3D::IsStartedUp() && Physics3D::Get().IsSimulating(); }

    uint64_t ScriptPhysics3D::Internal_GetCapabilities()
    {
        return Physics3D::IsStartedUp() ? static_cast<uint64_t>(Physics3D::Get().GetCapabilities()) : 0ull;
    }

    uint32_t ScriptPhysics3D::Internal_GetSubsteps()
    {
        return Physics3D::IsStartedUp() ? Physics3D::Get().GetSettings().Substeps : 1u;
    }

    void ScriptPhysics3D::Internal_SetSubsteps(uint32_t substeps)
    {
        if (!Physics3D::IsStartedUp())
            return;
        Physics3DSettings settings = Physics3D::Get().GetSettings();
        settings.Substeps = std::max(substeps, 1u);
        Physics3D::Get().SetSettings(settings);
    }

    MonoObject* ScriptPhysics3D::Internal_GetDefaultMaterial()
    {
        if (!Physics3D::IsStartedUp() || !ScriptAssetManager::IsStartedUp() || !Physics3D::Get().GetDefaultMaterial())
            return nullptr;
        ScriptAssetBase* const material = ScriptAssetManager::Get().GetScriptAsset(Physics3D::Get().GetDefaultMaterial(), true);
        return material ? material->GetManagedInstance() : nullptr;
    }

    void ScriptPhysics3D::Internal_SetDefaultMaterial(MonoObject* material)
    {
        if (!Physics3D::IsStartedUp())
            return;
        ScriptPhysicsMaterial3D* const nativeMaterial = ScriptPhysicsMaterial3D::ToNative(material);
        if (nativeMaterial)
            Physics3D::Get().SetDefaultMaterial(nativeMaterial->GetHandle());
    }

    void ScriptPhysics3D::Internal_GetGravity(glm::vec3* outGravity)
    {
        if (outGravity)
            *outGravity = Physics3D::IsStartedUp() ? Physics3D::Get().GetSettings().Gravity : glm::vec3(0.0f);
    }

    void ScriptPhysics3D::Internal_SetGravity(glm::vec3* gravity)
    {
        if (gravity && Physics3D::IsStartedUp())
            Physics3D::Get().SetGravity(*gravity);
    }

    bool ScriptPhysics3D::Internal_TrySetBackend(int32_t value)
    {
        if (!Physics3D::IsStartedUp() || Physics3D::Get().IsSimulating() || !IsValidBackend(value))
            return false;
        const auto backend = static_cast<Physics3DBackendType>(value);
        return Physics3D::IsBackendCompiled(backend) && Physics3D::Get().SetBackend(backend);
    }

    bool ScriptPhysics3D::Internal_IsBackendAvailable(int32_t value)
    {
        return IsValidBackend(value) && Physics3D::IsBackendCompiled(static_cast<Physics3DBackendType>(value));
    }

    MonoObject* ScriptPhysics3D::Internal_GetEntity(uint64_t entityId)
    {
        if (!SceneManager::TryGet() || !ScriptSceneObjectManager::IsStartedUp())
            return nullptr;
        const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        if (!scene || entityId > std::numeric_limits<uint32_t>::max())
            return nullptr;
        const Entity entity(static_cast<entt::entity>(static_cast<uint32_t>(entityId)), scene.get());
        if (!entity)
            return nullptr;
        ScriptEntity* const scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity);
        return scriptEntity ? scriptEntity->GetManagedInstance() : nullptr;
    }

    void ScriptPhysics3D::Internal_Raycast(glm::vec3* origin, glm::vec3* direction, float distance, uint32_t layerMask, bool includeTriggers,
                                           uint64_t ignoreBodyHandle, MonoArray** outResults)
    {
        if (!outResults)
            return;
        *outResults = nullptr;
        if (!origin || !direction || !Physics3D::IsStartedUp())
            return;

        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        SetQueryResults(Physics3D::Get().Raycast(*origin, *direction, distance, filter), outResults);
    }

    int32_t ScriptPhysics3D::Internal_RaycastNonAlloc(glm::vec3* origin, glm::vec3* direction, float distance, uint32_t layerMask,
                                                       bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray* results, int32_t capacity)
    {
        if (!origin || !direction || !results || !Physics3D::IsStartedUp())
            return 0;
        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        return WriteQueryResults(Physics3D::Get().Raycast(*origin, *direction, distance, filter), results, capacity);
    }

    void ScriptPhysics3D::Internal_Sweep(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                         glm::quat* rotation, glm::vec3* direction, float distance, uint32_t layerMask,
                                         bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray** outResults)
    {
        if (!outResults)
            return;
        *outResults = nullptr;
        if (!size || !position || !rotation || !direction || !Physics3D::IsStartedUp())
            return;

        PhysicsShape3DDesc shape;
        if (!MakeQueryShape(shapeType, *size, radius, height, shape))
            return;
        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        SetQueryResults(Physics3D::Get().Sweep(shape, *position, *rotation, *direction, distance, filter), outResults);
    }

    int32_t ScriptPhysics3D::Internal_SweepNonAlloc(int32_t shapeType, glm::vec3* size, float radius, float height,
                                                     glm::vec3* position, glm::quat* rotation, glm::vec3* direction, float distance,
                                                     uint32_t layerMask, bool includeTriggers, uint64_t ignoreBodyHandle,
                                                     MonoArray* results, int32_t capacity)
    {
        if (!size || !position || !rotation || !direction || !results || !Physics3D::IsStartedUp())
            return 0;
        PhysicsShape3DDesc shape;
        if (!MakeQueryShape(shapeType, *size, radius, height, shape))
            return 0;
        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        return WriteQueryResults(Physics3D::Get().Sweep(shape, *position, *rotation, *direction, distance, filter), results, capacity);
    }

    void ScriptPhysics3D::Internal_Overlap(int32_t shapeType, glm::vec3* size, float radius, float height, glm::vec3* position,
                                           glm::quat* rotation, uint32_t layerMask, bool includeTriggers, uint64_t ignoreBodyHandle,
                                           MonoArray** outResults)
    {
        if (!outResults)
            return;
        *outResults = nullptr;
        if (!size || !position || !rotation || !Physics3D::IsStartedUp())
            return;

        PhysicsShape3DDesc shape;
        if (!MakeQueryShape(shapeType, *size, radius, height, shape))
            return;
        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        SetQueryResults(Physics3D::Get().Overlap(shape, *position, *rotation, filter), outResults);
    }

    int32_t ScriptPhysics3D::Internal_OverlapNonAlloc(int32_t shapeType, glm::vec3* size, float radius, float height,
                                                       glm::vec3* position, glm::quat* rotation, uint32_t layerMask,
                                                       bool includeTriggers, uint64_t ignoreBodyHandle, MonoArray* results,
                                                       int32_t capacity)
    {
        if (!size || !position || !rotation || !results || !Physics3D::IsStartedUp())
            return 0;
        PhysicsShape3DDesc shape;
        if (!MakeQueryShape(shapeType, *size, radius, height, shape))
            return 0;
        PhysicsQueryFilter3D filter;
        filter.LayerMask = layerMask;
        filter.IncludeTriggers = includeTriggers;
        filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
        return WriteQueryResults(Physics3D::Get().Overlap(shape, *position, *rotation, filter), results, capacity);
    }
} // namespace Crowny
