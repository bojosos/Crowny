#include "cwpch.h"

#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics2D.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

#include <mono/metadata/object.h>

#include <algorithm>

namespace Crowny
{

    struct RaycastHit2DInterop
    {
        glm::vec2 Point;
        glm::vec2 Normal;
        float Fraction;
        uint32_t EntityId;
    };

    void ScriptPhysics2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetBackend", (void*)&Internal_GetBackend);
        MetaData.ScriptClass->AddInternalCall("Internal_IsSimulating", (void*)&Internal_IsSimulating);
        MetaData.ScriptClass->AddInternalCall("Internal_GetGravity", (void*)&Internal_GetGravity);
        MetaData.ScriptClass->AddInternalCall("Internal_SetGravity", (void*)&Internal_SetGravity);
        MetaData.ScriptClass->AddInternalCall("Internal_GetVelocityIterations", (void*)&Internal_GetVelocityIterations);
        MetaData.ScriptClass->AddInternalCall("Internal_SetVelocityIterations", (void*)&Internal_SetVelocityIterations);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPositionIterations", (void*)&Internal_GetPositionIterations);
        MetaData.ScriptClass->AddInternalCall("Internal_SetPositionIterations", (void*)&Internal_SetPositionIterations);
        MetaData.ScriptClass->AddInternalCall("Internal_GetDefaultMaterial", (void*)&Internal_GetDefaultMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_SetDefaultMaterial", (void*)&Internal_SetDefaultMaterial);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLayerName", (void*)&Internal_GetLayerName);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLayerName", (void*)&Internal_SetLayerName);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLayerMask", (void*)&Internal_GetLayerMask);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLayerMask", (void*)&Internal_SetLayerMask);
        MetaData.ScriptClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
        MetaData.ScriptClass->AddInternalCall("Internal_Raycast", (void*)&Internal_Raycast);
        MetaData.ScriptClass->AddInternalCall("Internal_RaycastNonAlloc", (void*)&Internal_RaycastNonAlloc);
    }

    Physics2DBackendType ScriptPhysics2D::Internal_GetBackend()
    {
        return Physics2D::TryGet() ? Physics2D::TryGet()->GetBackend() : Physics2DBackendType::Box2D;
    }

    bool ScriptPhysics2D::Internal_IsSimulating() { return Physics2D::TryGet() && Physics2D::TryGet()->IsSimulating(); }

    void ScriptPhysics2D::Internal_GetGravity(glm::vec2* outGravity)
    {
        if (outGravity)
            *outGravity = Physics2D::TryGet() ? Physics2D::TryGet()->GetGravity() : glm::vec2(0.0f);
    }

    void ScriptPhysics2D::Internal_SetGravity(glm::vec2* gravity)
    {
        if (Physics2D::TryGet() && gravity)
            Physics2D::TryGet()->SetGravity(*gravity);
    }

    uint32_t ScriptPhysics2D::Internal_GetVelocityIterations()
    {
        return Physics2D::TryGet() ? Physics2D::TryGet()->GetVelocityIterations() : 0u;
    }

    void ScriptPhysics2D::Internal_SetVelocityIterations(uint32_t iterations)
    {
        if (Physics2D::TryGet())
            Physics2D::TryGet()->SetVelocityIterations(iterations);
    }

    uint32_t ScriptPhysics2D::Internal_GetPositionIterations()
    {
        return Physics2D::TryGet() ? Physics2D::TryGet()->GetPositionIterations() : 0u;
    }

    void ScriptPhysics2D::Internal_SetPositionIterations(uint32_t iterations)
    {
        if (Physics2D::TryGet())
            Physics2D::TryGet()->SetPositionIterations(iterations);
    }

    MonoObject* ScriptPhysics2D::Internal_GetDefaultMaterial()
    {
        if (!Physics2D::TryGet() || !ScriptAssetManager::IsStartedUp())
            return nullptr;
        ScriptAssetBase* const asset = ScriptAssetManager::Get().GetScriptAsset(Physics2D::TryGet()->GetDefaultMaterial(), true);
        return asset ? asset->GetManagedInstance() : nullptr;
    }

    void ScriptPhysics2D::Internal_SetDefaultMaterial(MonoObject* material)
    {
        if (!Physics2D::TryGet())
            return;
        ScriptPhysicsMaterial2D* const scriptMaterial = ScriptPhysicsMaterial2D::ToNative(material);
        if (scriptMaterial && scriptMaterial->GetHandle())
            Physics2D::TryGet()->SetDefaultMaterial(scriptMaterial->GetHandle());
    }

    MonoString* ScriptPhysics2D::Internal_GetLayerName(int32_t layer)
    {
        if (!Physics2D::TryGet() || layer < 0 || layer >= static_cast<int32_t>(Physics2DLayerCount))
            return MonoUtils::ToMonoString(String());
        return MonoUtils::ToMonoString(Physics2D::TryGet()->GetLayerName(static_cast<uint32_t>(layer)));
    }

    void ScriptPhysics2D::Internal_SetLayerName(int32_t layer, MonoString* name)
    {
        if (Physics2D::TryGet() && name && layer >= 0 && layer < static_cast<int32_t>(Physics2DLayerCount))
            Physics2D::TryGet()->SetLayerName(static_cast<uint32_t>(layer), MonoUtils::FromMonoString(name));
    }

    uint32_t ScriptPhysics2D::Internal_GetLayerMask(int32_t layer)
    {
        return Physics2D::TryGet() && layer >= 0 && layer < static_cast<int32_t>(Physics2DLayerCount)
                 ? Physics2D::TryGet()->GetCategoryMask(static_cast<uint32_t>(layer))
                 : 0u;
    }

    void ScriptPhysics2D::Internal_SetLayerMask(int32_t layer, uint32_t mask)
    {
        if (Physics2D::TryGet() && layer >= 0 && layer < static_cast<int32_t>(Physics2DLayerCount))
            Physics2D::TryGet()->SetCategoryMask(static_cast<uint32_t>(layer), mask);
    }

    MonoObject* ScriptPhysics2D::Internal_GetEntity(uint32_t entityId)
    {
        if (!SceneManager::TryGet() || !ScriptSceneObjectManager::IsStartedUp())
            return nullptr;
        const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        if (!scene)
            return nullptr;
        const Entity entity(static_cast<entt::entity>(entityId), scene.get());
        if (!entity)
            return nullptr;
        ScriptEntity* const scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity);
        return scriptEntity ? scriptEntity->GetManagedInstance() : nullptr;
    }

    void ScriptPhysics2D::Internal_Raycast(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask, MonoArray** outResults)
    {
        if (!outResults)
            return;
        *outResults = nullptr;

        if (!origin || !direction || !Physics2D::TryGet())
            return;

        const Vector<PhysicsRaycastHit2D> hits = Physics2D::TryGet()->Raycast(*origin, *direction, distance, layerMask);
        if (hits.empty())
            return;

        MonoAssembly* const assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        MonoClass* const hitClass = assembly ? assembly->GetClass("Crowny", "RaycastHit2D") : nullptr;
        if (!hitClass)
        {
            CW_ENGINE_ERROR("Cannot find {0}.RaycastHit2D class.", CROWNY_NS);
            return;
        }

        *outResults = mono_array_new(MonoManager::Get().GetDomain(), hitClass->GetInternalPtr(), static_cast<uintptr_t>(hits.size()));

        for (size_t i = 0; i < hits.size(); ++i)
        {
            const RaycastHit2DInterop hit{ hits[i].Point, hits[i].Normal, hits[i].Fraction, static_cast<uint32_t>(hits[i].HitEntity.GetHandle()) };
            mono_array_set(*outResults, RaycastHit2DInterop, i, hit);
        }
    }

    int32_t ScriptPhysics2D::Internal_RaycastNonAlloc(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask,
                                                       MonoArray* results, int32_t capacity)
    {
        if (!origin || !direction || !results || !Physics2D::TryGet() || capacity <= 0)
            return 0;

        const Vector<PhysicsRaycastHit2D> hits = Physics2D::TryGet()->Raycast(*origin, *direction, distance, layerMask);
        const size_t count = std::min({ hits.size(), static_cast<size_t>(capacity), static_cast<size_t>(mono_array_length(results)) });
        for (size_t i = 0; i < count; ++i)
        {
            const RaycastHit2DInterop hit{ hits[i].Point, hits[i].Normal, hits[i].Fraction,
                                           static_cast<uint32_t>(hits[i].HitEntity.GetHandle()) };
            mono_array_set(results, RaycastHit2DInterop, i, hit);
        }
        return static_cast<int32_t>(count);
    }

} // namespace Crowny
