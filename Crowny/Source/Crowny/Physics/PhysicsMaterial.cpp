#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Scene/SceneManager.h"

#include <algorithm>
#include <cmath>

namespace Crowny
{
    namespace
    {
        bool IsValidCombineMode(PhysicsCombineMode value)
        {
            return static_cast<uint8_t>(value) <= static_cast<uint8_t>(PhysicsCombineMode::Maximum);
        }

        template <typename Material> void Refresh2DUsers(Material* changed)
        {
            if (!changed || !SceneManager::TryGet())
                return;
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            if (!scene)
                return;
            const bool isDefault = Physics2D::IsStartedUp() && Physics2D::TryGet()->GetDefaultMaterial().Get() == changed;

            for (const entt::entity handle : scene->GetAllEntitiesWith<BoxCollider2DComponent>())
            {
                auto& collider = Entity(handle, scene.get()).GetComponent<BoxCollider2DComponent>();
                if (collider.GetMaterial().Get() == changed || (isDefault && !collider.GetMaterial()))
                    collider.RefreshMaterial();
            }
            for (const entt::entity handle : scene->GetAllEntitiesWith<CircleCollider2DComponent>())
            {
                auto& collider = Entity(handle, scene.get()).GetComponent<CircleCollider2DComponent>();
                if (collider.GetMaterial().Get() == changed || (isDefault && !collider.GetMaterial()))
                    collider.RefreshMaterial();
            }
        }

        template <typename Collider> void Refresh3DUsers(Scene& scene, PhysicsMaterial3D* changed)
        {
            const bool isDefault = Physics3D::IsStartedUp() && Physics3D::Get().GetDefaultMaterial().Get() == changed;
            for (const entt::entity handle : scene.GetAllEntitiesWith<Collider>())
            {
                auto& collider = Entity(handle, &scene).GetComponent<Collider>();
                if (collider.GetMaterial().Get() == changed || (isDefault && !collider.GetMaterial()))
                    collider.RefreshMaterial();
            }
        }

        void Refresh3DUsers(PhysicsMaterial3D* changed)
        {
            if (!changed || !SceneManager::TryGet())
                return;
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            if (!scene)
                return;
            Refresh3DUsers<BoxCollider3DComponent>(*scene, changed);
            Refresh3DUsers<SphereCollider3DComponent>(*scene, changed);
            Refresh3DUsers<CapsuleCollider3DComponent>(*scene, changed);
        }

        bool MaterialDataEqual(const PhysicsMaterialData& first, const PhysicsMaterialData& second)
        {
            return first.Density == second.Density && first.Friction == second.Friction && first.Restitution == second.Restitution &&
                   first.RestitutionThreshold == second.RestitutionThreshold && first.FrictionCombine == second.FrictionCombine &&
                   first.RestitutionCombine == second.RestitutionCombine;
        }

        template <typename T> AssetHandle<T> CreateRuntimePhysicsMaterial(AssetManager& assetManager, const PhysicsMaterialData& data)
        {
            Ref<T> material = CreateRef<T>();
            material->SetData(data);
            return static_asset_cast<T>(assetManager.CreateAssetHandle(material));
        }
    } // namespace

    PhysicsMaterialData NormalizePhysicsMaterialData(const PhysicsMaterialData& material)
    {
        PhysicsMaterialData normalized = material;
        const PhysicsMaterialData defaults;
        if (!std::isfinite(normalized.Density))
            normalized.Density = defaults.Density;
        if (!std::isfinite(normalized.Friction))
            normalized.Friction = defaults.Friction;
        if (!std::isfinite(normalized.Restitution))
            normalized.Restitution = defaults.Restitution;
        if (!std::isfinite(normalized.RestitutionThreshold))
            normalized.RestitutionThreshold = defaults.RestitutionThreshold;
        normalized.Density = std::max(normalized.Density, 0.0f);
        normalized.Friction = std::max(normalized.Friction, 0.0f);
        normalized.Restitution = std::clamp(normalized.Restitution, 0.0f, 1.0f);
        normalized.RestitutionThreshold = std::max(normalized.RestitutionThreshold, 0.0f);
        if (!IsValidCombineMode(normalized.FrictionCombine))
            normalized.FrictionCombine = PhysicsCombineMode::GeometricMean;
        if (!IsValidCombineMode(normalized.RestitutionCombine))
            normalized.RestitutionCombine = PhysicsCombineMode::Maximum;
        return normalized;
    }

    PhysicsCombineMode ResolvePhysicsCombineMode(PhysicsCombineMode first, PhysicsCombineMode second)
    {
        if (!IsValidCombineMode(first))
            first = PhysicsCombineMode::GeometricMean;
        if (!IsValidCombineMode(second))
            second = PhysicsCombineMode::GeometricMean;
        return static_cast<uint8_t>(first) >= static_cast<uint8_t>(second) ? first : second;
    }

    float CombinePhysicsMaterialValue(float first, PhysicsCombineMode firstMode, float second, PhysicsCombineMode secondMode)
    {
        first = std::max(first, 0.0f);
        second = std::max(second, 0.0f);
        switch (ResolvePhysicsCombineMode(firstMode, secondMode))
        {
        case PhysicsCombineMode::GeometricMean:
            return std::sqrt(first * second);
        case PhysicsCombineMode::Average:
            return 0.5f * (first + second);
        case PhysicsCombineMode::Minimum:
            return std::min(first, second);
        case PhysicsCombineMode::Multiply:
            return first * second;
        case PhysicsCombineMode::Maximum:
            return std::max(first, second);
        default:
            return std::sqrt(first * second);
        }
    }

    void PhysicsMaterial2D::SetDensity(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Density = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetFriction(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Friction = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetRestitution(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Restitution = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetRestitutionThreshold(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.RestitutionThreshold = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetFrictionCombine(PhysicsCombineMode value)
    {
        PhysicsMaterialData data = m_Data;
        data.FrictionCombine = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetRestitutionCombine(PhysicsCombineMode value)
    {
        PhysicsMaterialData data = m_Data;
        data.RestitutionCombine = value;
        SetData(data);
    }

    void PhysicsMaterial2D::SetData(const PhysicsMaterialData& value)
    {
        const PhysicsMaterialData normalized = NormalizePhysicsMaterialData(value);
        if (MaterialDataEqual(m_Data, normalized))
            return;
        m_Data = normalized;
        NotifyChanged();
    }

    void PhysicsMaterial2D::NotifyChanged() { Refresh2DUsers(this); }

    void PhysicsMaterial3D::SetDensity(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Density = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetFriction(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Friction = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetRestitution(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.Restitution = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetRestitutionThreshold(float value)
    {
        PhysicsMaterialData data = m_Data;
        data.RestitutionThreshold = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetFrictionCombine(PhysicsCombineMode value)
    {
        PhysicsMaterialData data = m_Data;
        data.FrictionCombine = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetRestitutionCombine(PhysicsCombineMode value)
    {
        PhysicsMaterialData data = m_Data;
        data.RestitutionCombine = value;
        SetData(data);
    }

    void PhysicsMaterial3D::SetData(const PhysicsMaterialData& value)
    {
        const PhysicsMaterialData normalized = NormalizePhysicsMaterialData(value);
        if (MaterialDataEqual(m_Data, normalized))
            return;
        m_Data = normalized;
        NotifyChanged();
    }

    void PhysicsMaterial3D::NotifyChanged() { Refresh3DUsers(this); }

    AssetHandle<PhysicsMaterial2D> CreateRuntimePhysicsMaterial2D(AssetManager& assetManager, const PhysicsMaterialData& data)
    {
        return CreateRuntimePhysicsMaterial<PhysicsMaterial2D>(assetManager, data);
    }

    AssetHandle<PhysicsMaterial3D> CreateRuntimePhysicsMaterial3D(AssetManager& assetManager, const PhysicsMaterialData& data)
    {
        return CreateRuntimePhysicsMaterial<PhysicsMaterial3D>(assetManager, data);
    }
} // namespace Crowny
