#include "cwpch.h"

#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Serialization/PhysicsMaterial2DSerializer.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t PhysicsMaterialYamlVersion = 2;

        void SerializeMaterialData(const PhysicsMaterialData& material, YAML::Emitter& out)
        {
            out << YAML::Key << "Version" << YAML::Value << PhysicsMaterialYamlVersion;
            out << YAML::Key << "Density" << YAML::Value << material.Density;
            out << YAML::Key << "Friction" << YAML::Value << material.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << material.Restitution;
            out << YAML::Key << "RestitutionThreshold" << YAML::Value << material.RestitutionThreshold;
            out << YAML::Key << "FrictionCombine" << YAML::Value << static_cast<uint32_t>(material.FrictionCombine);
            out << YAML::Key << "RestitutionCombine" << YAML::Value << static_cast<uint32_t>(material.RestitutionCombine);
        }

        bool DeserializeMaterialData(const YAML::Node& node, PhysicsMaterialData& material)
        {
            if (!node || !node.IsMap())
                return false;
            try
            {
                const uint32_t version = node["Version"] ? node["Version"].as<uint32_t>() : 1;
                if (version > PhysicsMaterialYamlVersion)
                {
                    CW_ENGINE_ERROR("Physics material YAML version {} is newer than the supported version {}.", version,
                                    PhysicsMaterialYamlVersion);
                    return false;
                }
                if (node["Density"])
                    material.Density = node["Density"].as<float>();
                if (node["Friction"])
                    material.Friction = node["Friction"].as<float>();
                if (node["Restitution"])
                    material.Restitution = node["Restitution"].as<float>();
                if (node["RestitutionThreshold"])
                    material.RestitutionThreshold = node["RestitutionThreshold"].as<float>();
                if (node["FrictionCombine"])
                    material.FrictionCombine = static_cast<PhysicsCombineMode>(node["FrictionCombine"].as<uint32_t>());
                if (node["RestitutionCombine"])
                    material.RestitutionCombine = static_cast<PhysicsCombineMode>(node["RestitutionCombine"].as<uint32_t>());
                material = NormalizePhysicsMaterialData(material);
                return true;
            }
            catch (const YAML::Exception& error)
            {
                CW_ENGINE_ERROR("Failed to deserialize physics material: {}", error.what());
                return false;
            }
        }
    } // namespace

    void PhysicsMaterial2DSerializer::Serialize(const Ref<PhysicsMaterial2D>& material, YAML::Emitter& out)
    {
        if (!material)
            return;
        out << YAML::Comment("Crowny 2D Physics Material");
        out << YAML::BeginMap;
        SerializeMaterialData(material->m_Data, out);
        out << YAML::EndMap;
    }

    Ref<PhysicsMaterial2D> PhysicsMaterial2DSerializer::Deserialize(const YAML::Node& node)
    {
        PhysicsMaterialData data;
        if (!DeserializeMaterialData(node, data))
            return nullptr;
        Ref<PhysicsMaterial2D> material = CreateRef<PhysicsMaterial2D>();
        material->m_Data = data;
        return material;
    }

    void PhysicsMaterial3DSerializer::Serialize(const Ref<PhysicsMaterial3D>& material, YAML::Emitter& out)
    {
        if (!material)
            return;
        out << YAML::Comment("Crowny 3D Physics Material");
        out << YAML::BeginMap;
        SerializeMaterialData(material->m_Data, out);
        out << YAML::EndMap;
    }

    Ref<PhysicsMaterial3D> PhysicsMaterial3DSerializer::Deserialize(const YAML::Node& node)
    {
        PhysicsMaterialData data;
        if (!DeserializeMaterialData(node, data))
            return nullptr;
        Ref<PhysicsMaterial3D> material = CreateRef<PhysicsMaterial3D>();
        material->m_Data = data;
        return material;
    }
} // namespace Crowny
