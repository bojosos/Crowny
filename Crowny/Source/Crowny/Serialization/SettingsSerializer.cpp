#include "cwpch.h"

#include "Crowny/Serialization/SettingsSerializer.h"
#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{
	void TimeSettingsSerializer::Serialize(const Ref<TimeSettings>& settings, YAML::Emitter& out)
	{
		out << YAML::Comment("Crowny Time Settings");
		out << YAML::BeginMap;
		out << YAML::Key << "TimeScale" << YAML::Value << settings->TimeScale;
		out << YAML::Key << "MaxTimestep" << YAML::Value << settings->MaxTimestep;
		out << YAML::Key << "FixedTimestep" << YAML::Value << settings->FixedTimestep;
		out << YAML::EndMap;
	}

	Ref<TimeSettings> TimeSettingsSerializer::Deserialize(const YAML::Node& node)
	{
		Ref<TimeSettings> timeSettings = CreateRef<TimeSettings>();
		timeSettings->TimeScale = node["TimeScale"].as<float>(1.0f);
		timeSettings->MaxTimestep = node["MaxTimestep"].as<float>(1.0f / 3.0f);
		timeSettings->FixedTimestep = node["FixedTimestep"].as<float>(0.02f);
		return timeSettings;
	}

	void Physics2DSettingsSerializer::Serialize(const Ref<Physics2DSettings>& settings, YAML::Emitter& out)
	{
		out << YAML::Comment("Crowny Physics Settings");

		out << YAML::BeginMap;
		out << YAML::Key << "LayerNames" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < 32; i++)
			out << settings->LayerNames[i];
		out << YAML::EndSeq;
		out << YAML::Key << "Gravity2D" << YAML::Value << settings->Gravity;
		// out << YAML::Key << "DefaultMaterial" << YAML::Value << Physics2D::Get().GetDefaultMaterial();
		out << YAML::Key << "VelocityIterations" << YAML::Value << settings->VelocityIterations;
		out << YAML::Key << "PositionIterations" << YAML::Value << settings->PositionIterations;

		out << YAML::Key << "CollisionMatrix" << YAML::Value;
		out << YAML::BeginSeq;
		for (uint32_t i = 0; i < 32; i++)
			out << settings->MaskBits[i];
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}

	Ref<Physics2DSettings> Physics2DSettingsSerializer::Deserialize(const YAML::Node& node)
	{
		Ref<Physics2DSettings> physicsSettings = CreateRef<Physics2DSettings>();
		if (const auto& layerNames = node["LayerNames"])
		{
			uint32_t idx = 0;
			for (const auto& layerName : layerNames)
				physicsSettings->LayerNames[idx++] = layerName.as<String>();
		}
		physicsSettings->Gravity = node["Gravity2D"].as<glm::vec2>(glm::vec2(0.0f, -9.81f));
		physicsSettings->VelocityIterations = node["VelocityIterations"].as<uint32_t>(8);
		physicsSettings->PositionIterations = node["PositionIterations"].as<uint32_t>(3);

		const auto& collisionMatrix = node["CollisionMatrix"];
		if (collisionMatrix)
		{
			uint32_t i = 0;
			for (const auto& entry : collisionMatrix)
				physicsSettings->MaskBits[i++] = entry.as<uint32_t>();
		}
		return physicsSettings ;
	}
}