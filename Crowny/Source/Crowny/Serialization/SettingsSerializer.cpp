#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Serialization/SettingsSerializer.h"

namespace Crowny
{
    namespace
    {
        template <typename T> AssetHandle<T> LoadSettingsAssetReference(const UUID& uuid)
        {
            if (uuid == UUID::EMPTY || AssetManager::TryGet() == nullptr)
                return {};
            AssetHandle<T> asset = AssetManager::TryGet()->LoadFromUUID<T>(uuid);
            return asset.HasUUID() ? asset : static_asset_cast<T>(AssetManager::TryGet()->GetAssetHandle(uuid));
        }

        template <typename T> UUID SerializeSettingsAssetReference(const AssetHandle<T>& asset)
        {
            if (!asset.HasUUID())
                return UUID::EMPTY;
            if (!asset.IsLoaded())
                return asset.GetUUID();
            Path path;
            return AssetManager::TryGet() != nullptr && AssetManager::TryGet()->GetAssetPath(asset.GetUUID(), path) ? asset.GetUUID() : UUID::EMPTY;
        }
    } // namespace

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

        SerializeValueYAML(out, "Backend", static_cast<uint32_t>(settings->Backend));

        SerializeValueYAML(out, "LayerNames", YAML::BeginSeq);
        for (uint32_t i = 0; i < 32; i++)
            SerializeValueYAML(out, settings->LayerNames[i]);
        EndYAMLSeq(out);

        SerializeValueYAML(out, "Gravity2D", settings->Gravity);
        SerializeValueYAML(out, "DefaultMaterial", SerializeSettingsAssetReference(settings->DefaultMaterial));
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
        physicsSettings->MaskBits.fill(0xFFFFu);

        const YAML::Node& physicsSettingsNode = node["PhysicsSettings"];
        if (!physicsSettingsNode)
            return physicsSettings;

        uint32_t backend = static_cast<uint32_t>(Physics2DBackendType::Box2D);
        DeserializeValueYAML(physicsSettingsNode, "Backend", backend, backend);
        if (backend <= static_cast<uint32_t>(Physics2DBackendType::Box2D))
            physicsSettings->Backend = static_cast<Physics2DBackendType>(backend);

        if (const auto& layerNames = physicsSettingsNode["LayerNames"])
        {
            uint32_t idx = 0;
            for (const auto& layerName : layerNames)
            {
                if (idx >= physicsSettings->LayerNames.size())
                    break;
                physicsSettings->LayerNames[idx++] = layerName.as<String>();
            }
        }
        DeserializeValueYAML(physicsSettingsNode, "Gravity2D", physicsSettings->Gravity, glm::vec2(0.0f, -9.81f));
        const UUID defaultMaterial = physicsSettingsNode["DefaultMaterial"].as<UUID>(UUID::EMPTY);
        if (defaultMaterial != UUID::EMPTY)
            physicsSettings->DefaultMaterial = LoadSettingsAssetReference<PhysicsMaterial2D>(defaultMaterial);
        DeserializeValueYAML(physicsSettingsNode, "VelocityIterations", physicsSettings->VelocityIterations, 8U);
        DeserializeValueYAML(physicsSettingsNode, "PositionIterations", physicsSettings->PositionIterations, 3U);

        if (const auto& collisionMatrix = physicsSettingsNode["CollisionMatrix"])
        {
            uint32_t i = 0;
            for (const auto& entry : collisionMatrix)
            {
                if (i >= physicsSettings->MaskBits.size())
                    break;
                physicsSettings->MaskBits[i++] = entry.as<uint32_t>();
            }
        }
        return physicsSettings;
    }

    void Physics3DSettingsSerializer::Serialize(const Physics3DSettings& settings, YAML::Emitter& out)
    {
        BeginYAMLMap(out, "Physics3DSettings");
        SerializeEnumYAML(out, "Backend", settings.Backend);
        SerializeValueYAML(out, "Gravity", settings.Gravity);
        SerializeValueYAML(out, "DefaultMaterial", SerializeSettingsAssetReference(settings.DefaultMaterial));
        SerializeValueYAML(out, "Substeps", settings.Substeps);
        SerializeValueYAML(out, "EnableSleeping", settings.EnableSleeping);
        SerializeValueYAML(out, "EnableContinuousCollision", settings.EnableContinuousCollision);
        SerializeValueYAML(out, "Deterministic", settings.Deterministic);
        EndYAMLMap(out, "Physics3DSettings");
    }

    Physics3DSettings Physics3DSettingsSerializer::Deserialize(const YAML::Node& node)
    {
        Physics3DSettings settings;
        const YAML::Node& settingsNode = node["Physics3DSettings"];
        if (!settingsNode)
            return settings;

        uint32_t backend = static_cast<uint32_t>(Physics3DBackendType::Box3D);
        DeserializeValueYAML(settingsNode, "Backend", backend, backend);
        if (backend <= static_cast<uint32_t>(Physics3DBackendType::Bullet) &&
            Physics3D::IsBackendCompiled(static_cast<Physics3DBackendType>(backend)))
            settings.Backend = static_cast<Physics3DBackendType>(backend);
        DeserializeValueYAML(settingsNode, "Gravity", settings.Gravity, glm::vec3(0.0f, -9.81f, 0.0f));
        settings.DefaultMaterial = LoadSettingsAssetReference<PhysicsMaterial3D>(settingsNode["DefaultMaterial"].as<UUID>(UUID::EMPTY));
        DeserializeValueYAML(settingsNode, "Substeps", settings.Substeps, 4u);
        DeserializeValueYAML(settingsNode, "EnableSleeping", settings.EnableSleeping, true);
        DeserializeValueYAML(settingsNode, "EnableContinuousCollision", settings.EnableContinuousCollision, true);
        DeserializeValueYAML(settingsNode, "Deterministic", settings.Deterministic, false);
        settings.Substeps = std::max(settings.Substeps, 1u);
        return settings;
    }
} // namespace Crowny
