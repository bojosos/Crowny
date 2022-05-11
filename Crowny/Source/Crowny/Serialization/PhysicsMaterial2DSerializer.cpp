#include "cwpch.h"

#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Serialization/PhysicsMaterial2DSerializer.h"

#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{

    void PhysicsMaterial2DSerializer::Serialize(const Ref<PhysicsMaterial2D>& material, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny Physics Material");
        out << YAML::BeginMap;
        out << YAML::Key << "Density" << YAML::Value << material->m_Density;
        out << YAML::Key << "Friction" << YAML::Value << material->m_Friction;
        out << YAML::Key << "Restitution" << YAML::Value << material->m_Restitution;
        out << YAML::Key << "RestitutionThreshold" << YAML::Value << material->m_RestitutionThreshold;
        out << YAML::EndMap;
    }

    Ref<PhysicsMaterial2D> PhysicsMaterial2DSerializer::Deserialize(const YAML::Node& node)
    {
        Ref<PhysicsMaterial2D> material = CreateRef<PhysicsMaterial2D>();
        material->m_Density = node["Density"].as<float>(1.0f);
        material->m_Friction = node["Friction"].as<float>(0.5f);
        material->m_Restitution = node["Restitution"].as<float>(0.0f);
        material->m_RestitutionThreshold = node["RestitutionThreshold"].as<float>(0.5f);
        return material;
    }

} // namespace Crowny