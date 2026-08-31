#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/Flags.h"

#include <cstdint>

namespace Crowny
{
    class PhysicsMaterial2DSerializer;
    class PhysicsMaterial3DSerializer;
    class AssetManager;

    enum class PhysicsCombineMode : uint8_t
    {
        GeometricMean = 0,
        Average = 1,
        Minimum = 2,
        Multiply = 3,
        Maximum = 4
    };

    struct PhysicsMaterialData
    {
        float Density = 1.0f;
        float Friction = 0.5f;
        float Restitution = 0.0f;
        float RestitutionThreshold = 0.5f;
        PhysicsCombineMode FrictionCombine = PhysicsCombineMode::GeometricMean;
        PhysicsCombineMode RestitutionCombine = PhysicsCombineMode::Maximum;
    };

    enum class PhysicsMaterialOverrideBits : uint32_t
    {
        None = 0,
        Density = 1u << 0u,
        Friction = 1u << 1u,
        Restitution = 1u << 2u,
        RestitutionThreshold = 1u << 3u,
        FrictionCombine = 1u << 4u,
        RestitutionCombine = 1u << 5u,
        All = (1u << 6u) - 1u
    };

    using PhysicsMaterialOverrideFlags = Flags<PhysicsMaterialOverrideBits>;
    CW_FLAGS_OPERATORS(PhysicsMaterialOverrideBits)

    struct PhysicsMaterialOverride
    {
        PhysicsMaterialOverrideFlags Fields;
        PhysicsMaterialData Values;
    };

    PhysicsMaterialData NormalizePhysicsMaterialData(const PhysicsMaterialData& material);
    PhysicsMaterialOverride NormalizePhysicsMaterialOverride(const PhysicsMaterialOverride& materialOverride);
    PhysicsMaterialData ResolvePhysicsMaterialData(const PhysicsMaterialData& baseMaterial,
                                                   const PhysicsMaterialOverride& materialOverride);
    PhysicsMaterialOverride MakePhysicsMaterialOverride(const PhysicsMaterialData& material,
                                                        PhysicsMaterialOverrideFlags fields = PhysicsMaterialOverrideBits::All);
    PhysicsCombineMode ResolvePhysicsCombineMode(PhysicsCombineMode first, PhysicsCombineMode second);
    float CombinePhysicsMaterialValue(float first, PhysicsCombineMode firstMode, float second, PhysicsCombineMode secondMode);

    class PhysicsMaterial2D : public Asset
    {
    public:
        PhysicsMaterial2D() = default;

        AssetType GetAssetType() const override { return AssetType::PhysicsMaterial2D; }
        static AssetType GetStaticType() { return AssetType::PhysicsMaterial2D; }

        float GetDensity() const { return m_Data.Density; }
        float GetFriction() const { return m_Data.Friction; }
        float GetRestitution() const { return m_Data.Restitution; }
        float GetRestitutionThreshold() const { return m_Data.RestitutionThreshold; }
        PhysicsCombineMode GetFrictionCombine() const { return m_Data.FrictionCombine; }
        PhysicsCombineMode GetRestitutionCombine() const { return m_Data.RestitutionCombine; }
        const PhysicsMaterialData& GetData() const { return m_Data; }

        void SetDensity(float value);
        void SetFriction(float value);
        void SetRestitution(float value);
        void SetRestitutionThreshold(float value);
        void SetFrictionCombine(PhysicsCombineMode value);
        void SetRestitutionCombine(PhysicsCombineMode value);
        void SetData(const PhysicsMaterialData& value);

    private:
        void NotifyChanged();

        CW_SERIALIZABLE(PhysicsMaterial2D);
        using Serializer = PhysicsMaterial2DSerializer;

        friend class PhysicsMaterial2DSerializer;
        friend class AssetManager;

        PhysicsMaterialData m_Data;
    };

    class PhysicsMaterial3D : public Asset
    {
    public:
        PhysicsMaterial3D() = default;

        AssetType GetAssetType() const override { return AssetType::PhysicsMaterial; }
        static AssetType GetStaticType() { return AssetType::PhysicsMaterial; }

        float GetDensity() const { return m_Data.Density; }
        float GetFriction() const { return m_Data.Friction; }
        float GetRestitution() const { return m_Data.Restitution; }
        float GetRestitutionThreshold() const { return m_Data.RestitutionThreshold; }
        PhysicsCombineMode GetFrictionCombine() const { return m_Data.FrictionCombine; }
        PhysicsCombineMode GetRestitutionCombine() const { return m_Data.RestitutionCombine; }
        const PhysicsMaterialData& GetData() const { return m_Data; }

        void SetDensity(float value);
        void SetFriction(float value);
        void SetRestitution(float value);
        void SetRestitutionThreshold(float value);
        void SetFrictionCombine(PhysicsCombineMode value);
        void SetRestitutionCombine(PhysicsCombineMode value);
        void SetData(const PhysicsMaterialData& value);

    private:
        void NotifyChanged();

        CW_SERIALIZABLE(PhysicsMaterial3D);
        using Serializer = PhysicsMaterial3DSerializer;

        friend class PhysicsMaterial3DSerializer;
        friend class AssetManager;

        PhysicsMaterialData m_Data;
    };

    AssetHandle<PhysicsMaterial2D> CreateRuntimePhysicsMaterial2D(AssetManager& assetManager,
                                                                  const PhysicsMaterialData& data = {});
    AssetHandle<PhysicsMaterial3D> CreateRuntimePhysicsMaterial3D(AssetManager& assetManager,
                                                                  const PhysicsMaterialData& data = {});
} // namespace Crowny
