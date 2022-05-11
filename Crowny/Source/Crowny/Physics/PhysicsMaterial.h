#pragma once

#include "Crowny/Assets/Asset.h"

namespace Crowny
{

    class Physics2D;
    class PhysicsMaterial2DSerializer;

    class PhysicsMaterial2D : public Asset
    {
    public:
        PhysicsMaterial2D() = default;

        virtual AssetType GetAssetType() const override { return AssetType::PhysicsMaterial2D; }
        static AssetType GetStaticType() { return AssetType::PhysicsMaterial2D; }

        float& GetDensity() { return m_Density; }
        float& GetFriction() { return m_Friction; }
        float& GetRestitution() { return m_Restitution; }
        float& GetRestitutionThreshold() { return m_RestitutionThreshold; }

    private:
        CW_SERIALIZABLE(PhysicsMaterial2D);
        using Serializer = PhysicsMaterial2DSerializer;

        friend class Physics2D;
        friend class PhysicsMaterial2DSerializer;

        float m_Density = 1.0f;
        float m_Friction = 0.5f;
        float m_Restitution = 0.0f;
        float m_RestitutionThreshold = 0.5f;
    };

} // namespace Crowny