#pragma once

#include "Crowny/Assets/Asset.h"

namespace Crowny
{

	class Physics2D;
	
    class PhysicsMaterial2D : public Asset
    {
	public:
		PhysicsMaterial2D() = default;
		
		virtual AssetType GetAssetType() const override { return AssetType::PhysicsMaterial2D; }
		static AssetType GetStaticType() { return AssetType::PhysicsMaterial2D; }
		
	private:
		friend class Physics2D;
		friend static void DrawPhysicsMaterial(PhysicsMaterial2D& material);

        float m_Density = 1.0f;
        float m_Friction = 0.5f;
        float m_Restitution = 0.0f;
        float m_RestitutionThreshold = 0.5f;
    };

} // namespace Crowny