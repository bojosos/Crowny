#include "cwpch.h"

#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Serialization/SettingsSerializer.h"

namespace Crowny
{
    void TimeSettingsSerializer::Serialize(const Ref<TimeSettings>& settings, YAML::Emitter& out)
    {
        BeginYAMLMap(out, "TimeSettings");

        SerializeValueYAML(out, "TimeScale", settings->TimeScale);
        SerializeValueYAML(out, "MaxTimestep", settings->MaxTimestep);
        SerializeValueYAML(out, "FixedTimestep", settings->FixedTimestep);

        EndYAMLMap(out, "TimeSettings");
    }

    Ref<TimeSettings> TimeSettingsSerializer::Deserialize(const YAML::Node& node)
    {
        Ref<TimeSettings> timeSettings = CreateRef<TimeSettings>();

        const YAML::Node& timeSettingsNode = node["TimeSettings"];
        if (!timeSettingsNode)
            return timeSettings;

        DeserializeValueYAML(timeSettingsNode, "TimeScale", timeSettings->TimeScale, 1.0f);
        DeserializeValueYAML(timeSettingsNode, "MaxTimestep", timeSettings->MaxTimestep, 1.0f / 3.0f);
        DeserializeValueYAML(timeSettingsNode, "FixedTimestep", timeSettings->FixedTimestep, 0.02f);

        return timeSettings;
    }

    void Physics2DSettingsSerializer::Serialize(const Ref<Physics2DSettings>& settings, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny Physics Settings");

        BeginYAMLMap(out, "PhysicsSettings");

        SerializeValueYAML(out, "LayerNames", YAML::BeginSeq);
        for (uint32_t i = 0; i < 32; i++)
            SerializeValueYAML(out, settings->LayerNames[i]);
        EndYAMLSeq(out);

        SerializeValueYAML(out, "Gravity2D", settings->Gravity);
        // out << YAML::Key << "DefaultMaterial" << YAML::Value << Physics2D::Get().GetDefaultMaterial();
        SerializeValueYAML(out, "VelocityIterations", settings->VelocityIterations);
        SerializeValueYAML(out, "PositionIterations", settings->PositionIterations);

        SerializeValueYAML(out, "CollisionMatrix", YAML::BeginSeq);
        for (uint32_t i = 0; i < 32; i++)
            SerializeValueYAML(out, settings->MaskBits[i]);
        EndYAMLSeq(out);

        EndYAMLMap(out, "PhysicsSettings");
    }

    Ref<Physics2DSettings> Physics2DSettingsSerializer::Deserialize(const YAML::Node& node)
    {
        Ref<Physics2DSettings> physicsSettings = CreateRef<Physics2DSettings>();

        const YAML::Node& physicsSettingsNode = node["PhysicsSettings"];
        if (!physicsSettings)
            return physicsSettings;

        if (const auto& layerNames = node["LayerNames"])
        {
            uint32_t idx = 0;
            for (const auto& layerName : layerNames)
                physicsSettings->LayerNames[idx++] = layerName.as<String>();
        }
        DeserializeValueYAML(node, "Gravity2D", physicsSettings->Gravity, glm::vec2(0.0f, -9.81f));
        DeserializeValueYAML(node, "VelocityIterations", physicsSettings->VelocityIterations, 8U);
        DeserializeValueYAML(node, "PositionIterations", physicsSettings->PositionIterations, 3U);

        if (const auto& collisionMatrix = node["CollisionMatrix"])
        {
            uint32_t i = 0;
            for (const auto& entry : collisionMatrix)
                physicsSettings->MaskBits[i++] = entry.as<uint32_t>();
        }
        return physicsSettings;
    }
} // namespace Crowny