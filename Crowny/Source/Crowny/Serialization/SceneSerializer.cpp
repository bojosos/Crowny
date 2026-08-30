#include "cwpch.h"

#include "Crowny/Serialization/SceneSerializer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SceneComponentCodec.h"
#include "Crowny/Serialization/SettingsSerializer.h"

#include <cereal/types/string.hpp>

namespace Crowny
{
    namespace
    {
        template <typename T> AssetHandle<T> LoadAssetReference(const UUID& uuid)
        {
            if (uuid == UUID::EMPTY || AssetManager::TryGet() == nullptr)
                return {};
            AssetHandle<T> asset = AssetManager::TryGet()->LoadFromUUID<T>(uuid);
            if (asset.HasUUID())
                return asset;
            return static_asset_cast<T>(AssetManager::TryGet()->GetAssetHandle(uuid));
        }

        template <typename T> bool ShouldSerializeMaterialReference(const AssetHandle<T>& material)
        {
            if (!material.HasUUID())
                return false;
            if (!material.IsLoaded())
                return true;
            Path path;
            return AssetManager::TryGet() != nullptr && AssetManager::TryGet()->GetAssetPath(material.GetUUID(), path);
        }

        YAML::Node FindComponentNode(const YAML::Node& entityNode, const SceneComponentCodec& codec)
        {
            return entityNode[codec.YamlName];
        }

        void ResolveRelationships(Scene& scene, UnorderedMap<Entity, Vector<UUID>>& relationships)
        {
            for (auto& [entity, childUuids] : relationships)
            {
                auto& relationship = entity.GetComponent<RelationshipComponent>();
                relationship.Parent = {};
                relationship.Children.clear();
                relationship.SiblingIndex = 0;
                relationship.Children.reserve(childUuids.size());
            }

            for (auto& [parent, childUuids] : relationships)
            {
                auto& relationship = parent.GetComponent<RelationshipComponent>();
                for (const UUID& childUuid : childUuids)
                {
                    Entity child = scene.GetEntityFromUuid(childUuid);
                    if (!child)
                        continue;
                    auto& childRelationship = child.GetComponent<RelationshipComponent>();
                    childRelationship.Parent = parent;
                    childRelationship.SiblingIndex = static_cast<uint32_t>(relationship.Children.size());
                    relationship.Children.push_back(child);
                }
            }
        }
    } // namespace

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        if (!entity)
            return;

        out << YAML::BeginMap;
        SerializeValueYAML(out, "Entity", entity.GetUuid());
        for (const SceneComponentCodec& codec : GetSceneComponentCodecs())
        {
            if (!codec.HasComponent(entity) || !codec.ShouldSerialize(entity))
                continue;
            if (codec.YamlType == SceneComponentYamlType::Null)
            {
                SerializeValueYAML(out, codec.YamlName, YAML::Null);
                continue;
            }
            BeginYAMLMap(out, codec.YamlName);
            codec.WriteYaml(out, entity);
            EndYAMLMap(out, codec.YamlName);
        }
        EndYAMLMap(out, "Entity");
    }

    void SceneSerializer::DeserializeEntities(const YAML::Node& entitiesNode)
    {
        UnorderedMap<Entity, Vector<UUID>> relationships;
        SceneComponentReadContext context{ m_Scene.get(), &relationships };

        for (const YAML::Node& entityNode : entitiesNode)
        {
            const UUID uuid = entityNode["Entity"].as<UUID>();
            String name;
            if (const SceneComponentCodec* tagCodec = FindSceneComponentCodec(SceneComponentId::Tag))
            {
                const YAML::Node tagNode = FindComponentNode(entityNode, *tagCodec);
                if (tagNode)
                    name = tagNode["Tag"].as<String>("");
            }

            Entity entity = m_Scene->CreateEntityWithUuid(uuid, name);
            for (const SceneComponentCodec& codec : GetSceneComponentCodecs())
            {
                const YAML::Node componentNode = FindComponentNode(entityNode, codec);
                if (!componentNode)
                    continue;
                codec.ReadYaml(componentNode, entity, context);
            }
        }

        ResolveRelationships(*m_Scene, relationships);
    }

    void SceneSerializer::Serialize(const Path& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", FORMAT_VERSION);
        SerializeValueYAML(out, "Scene", m_Scene->GetName());
        SerializeValueYAML(out, "ImGuiLayout", m_Scene->GetImGuiLayout());
        SerializeValueYAML(out, "Entities", YAML::BeginSeq);
        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
        m_Scene->m_Registry.each([&](auto entityId) { SerializeEntity(out, { entityId, m_Scene.get() }); });
        out << YAML::EndSeq;
        TimeSettingsSerializer::Serialize(Application::TryGet()->GetTimeSettings(), out);
        Physics2DSettingsSerializer::Serialize(Physics2D::TryGet()->GetPhysicsSettings(), out);
        if (Physics3D::IsStartedUp())
            Physics3DSettingsSerializer::Serialize(Physics3D::Get().GetSettings(), out);
        out << YAML::EndMap;

        m_Scene->m_Filepath = filepath;
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        const char* text = out.c_str();
        stream->Write(text, std::strlen(text));
        stream->Close();
    }

    void SceneSerializer::SerializeBinary(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        BinaryDataStreamOutputArchive archive(stream);

        archive(static_cast<uint32_t>(FORMAT_VERSION));
        archive(m_Scene->GetName(), m_Scene->GetImGuiLayout());

        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
        uint32_t entityCount = 0;
        m_Scene->m_Registry.each([&](auto) { entityCount++; });
        archive(entityCount);

        m_Scene->m_Registry.each([&](auto entityId) {
            Entity entity = { entityId, m_Scene.get() };
            if (!entity)
                return;

            archive(entity.GetUuid(), entity.GetName());
            uint32_t componentCount = 0;
            for (const SceneComponentCodec& codec : GetSceneComponentCodecs())
            {
                if (codec.HasComponent(entity) && codec.ShouldSerialize(entity))
                    componentCount++;
            }
            archive(componentCount);
            for (const SceneComponentCodec& codec : GetSceneComponentCodecs())
            {
                if (!codec.HasComponent(entity) || !codec.ShouldSerialize(entity))
                    continue;
                archive(static_cast<uint32_t>(codec.Id));
                codec.WriteBinary(archive, entity);
            }
        });

        const auto& timeSettings = Application::TryGet()->GetTimeSettings();
        archive(timeSettings->TimeScale, timeSettings->MaxTimestep, timeSettings->FixedTimestep);

        const auto& physics2DSettings = Physics2D::TryGet()->GetPhysicsSettings();
        archive(physics2DSettings->Gravity.x, physics2DSettings->Gravity.y);
        archive(physics2DSettings->VelocityIterations, physics2DSettings->PositionIterations);
        for (uint32_t index = 0; index < 32; index++)
            archive(physics2DSettings->LayerNames[index]);
        for (uint32_t index = 0; index < 32; index++)
            archive(physics2DSettings->MaskBits[index]);
        archive(ShouldSerializeMaterialReference(physics2DSettings->DefaultMaterial) ? physics2DSettings->DefaultMaterial.GetUUID() : UUID::EMPTY);

        const Physics3DSettings physics3DSettings = Physics3D::IsStartedUp() ? Physics3D::Get().GetSettings() : Physics3DSettings{};
        archive(static_cast<uint32_t>(physics3DSettings.Backend));
        archive(physics3DSettings.Gravity.x, physics3DSettings.Gravity.y, physics3DSettings.Gravity.z);
        archive(physics3DSettings.Substeps, physics3DSettings.EnableSleeping, physics3DSettings.EnableContinuousCollision,
                physics3DSettings.Deterministic);
        archive(ShouldSerializeMaterialReference(physics3DSettings.DefaultMaterial) ? physics3DSettings.DefaultMaterial.GetUUID() : UUID::EMPTY);

        m_Scene->m_Filepath = filepath;
        stream->Close();
    }

    bool SceneSerializer::Deserialize(const Path& filepath)
    {
        const Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (stream == nullptr)
        {
            CW_ENGINE_ERROR("Could not open scene \"{0}\".", filepath);
            return false;
        }
        const String text = stream->GetAsString();
        stream->Close();
        bool sceneMutationStarted = false;
        try
        {
            YAML::Node data = YAML::Load(text);
            if (!data["Scene"])
                return false;
            const uint32_t version = data["Version"].as<uint32_t>(0);
            if (version != FORMAT_VERSION)
            {
                CW_ENGINE_ERROR("Scene '{}' uses version {}, but this build requires version {}.", filepath, version, FORMAT_VERSION);
                return false;
            }

            sceneMutationStarted = true;
            m_Scene->m_Registry.clear();
            m_Scene->m_EntityMap.clear();
            delete m_Scene->m_RootEntity;
            m_Scene->m_RootEntity = nullptr;
            m_Scene->m_Name = data["Scene"].as<String>();
            m_Scene->m_ImGuiLayout = data["ImGuiLayout"].as<String>("");
            m_Scene->m_Filepath = filepath;
            if (data["Entities"])
                DeserializeEntities(data["Entities"]);

            auto view = m_Scene->GetAllEntitiesWith<TagComponent>();
            Entity root;
            for (auto handle : view)
            {
                Entity entity = { handle, m_Scene.get() };
                if (!entity.GetParent())
                {
                    root = entity;
                    break;
                }
            }
            if (root)
                m_Scene->m_RootEntity = new Entity(root.GetHandle(), m_Scene.get());
            else
                m_Scene->CreateRootEntity();
            if (m_Scene->m_RootEntity)
                m_Scene->m_RootEntity->NotifyTransformChanged();
            if (Application::IsStartedUp())
                Application::TryGet()->SetTimeSettings(TimeSettingsSerializer::Deserialize(data));
            if (Physics2D::TryGet() != nullptr)
                Physics2D::TryGet()->SetPhysicsSettings(Physics2DSettingsSerializer::Deserialize(data));
            if (Physics3D::IsStartedUp())
                Physics3D::Get().SetSettings(Physics3DSettingsSerializer::Deserialize(data));
            return true;
        }
        catch (const std::exception& exception)
        {
            if (sceneMutationStarted)
            {
                m_Scene->m_Registry.clear();
                m_Scene->m_EntityMap.clear();
                delete m_Scene->m_RootEntity;
                m_Scene->m_RootEntity = nullptr;
            }
            if (m_Scene->m_RootEntity == nullptr)
                m_Scene->CreateRootEntity();
            CW_ENGINE_ERROR("Error deserializing scene \"{0}\". {1}.", filepath, std::string(exception.what()));
            return false;
        }
    }

    bool SceneSerializer::DeserializeBinary(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (stream == nullptr)
        {
            CW_ENGINE_ERROR("Could not open binary scene \"{0}\".", filepath);
            return false;
        }

        BinaryDataStreamInputArchive archive(stream);
        bool sceneMutationStarted = false;
        try
        {
            uint32_t version;
            archive(version);
            if (version != FORMAT_VERSION)
            {
                CW_ENGINE_ERROR("Binary scene '{}' uses version {}, but this build requires version {}.", filepath, version, FORMAT_VERSION);
                stream->Close();
                return false;
            }

            String sceneName;
            String layout;
            archive(sceneName, layout);
            sceneMutationStarted = true;
            m_Scene->m_Registry.clear();
            m_Scene->m_EntityMap.clear();
            delete m_Scene->m_RootEntity;
            m_Scene->m_RootEntity = nullptr;
            m_Scene->m_Name = sceneName;
            m_Scene->m_ImGuiLayout = layout;
            m_Scene->m_Filepath = filepath;

            uint32_t entityCount;
            archive(entityCount);
            UnorderedMap<Entity, Vector<UUID>> relationships;
            SceneComponentReadContext context{ m_Scene.get(), &relationships };
            for (uint32_t entityIndex = 0; entityIndex < entityCount; entityIndex++)
            {
                UUID uuid;
                String name;
                uint32_t componentCount;
                archive(uuid, name, componentCount);
                Entity entity = m_Scene->CreateEntityWithUuid(uuid, name);
                for (uint32_t componentIndex = 0; componentIndex < componentCount; componentIndex++)
                {
                    uint32_t stableId;
                    archive(stableId);
                    const SceneComponentCodec* codec = FindSceneComponentCodec(stableId);
                    if (codec == nullptr)
                        throw std::runtime_error("Unknown binary component ID " + std::to_string(stableId));
                    codec->ReadBinary(archive, entity, context);
                }
            }
            ResolveRelationships(*m_Scene, relationships);

            auto view = m_Scene->GetAllEntitiesWith<TagComponent>();
            Entity root;
            for (auto handle : view)
            {
                Entity entity = { handle, m_Scene.get() };
                if (!entity.GetParent())
                {
                    root = entity;
                    break;
                }
            }
            if (root)
                m_Scene->m_RootEntity = new Entity(root.GetHandle(), m_Scene.get());
            else
                m_Scene->CreateRootEntity();
            if (m_Scene->m_RootEntity)
                m_Scene->m_RootEntity->NotifyTransformChanged();

            Ref<TimeSettings> timeSettings = CreateRef<TimeSettings>();
            archive(timeSettings->TimeScale, timeSettings->MaxTimestep, timeSettings->FixedTimestep);
            if (Application::IsStartedUp())
                Application::TryGet()->SetTimeSettings(timeSettings);

            Ref<Physics2DSettings> physics2DSettings = CreateRef<Physics2DSettings>();
            archive(physics2DSettings->Gravity.x, physics2DSettings->Gravity.y, physics2DSettings->VelocityIterations,
                    physics2DSettings->PositionIterations);
            for (uint32_t index = 0; index < 32; index++)
                archive(physics2DSettings->LayerNames[index]);
            for (uint32_t index = 0; index < 32; index++)
                archive(physics2DSettings->MaskBits[index]);
            UUID defaultMaterial2D;
            archive(defaultMaterial2D);
            physics2DSettings->DefaultMaterial = LoadAssetReference<PhysicsMaterial2D>(defaultMaterial2D);
            if (Physics2D::TryGet() != nullptr)
                Physics2D::TryGet()->SetPhysicsSettings(physics2DSettings);

            Physics3DSettings physics3DSettings;
            uint32_t backend;
            archive(backend);
            archive(physics3DSettings.Gravity.x, physics3DSettings.Gravity.y, physics3DSettings.Gravity.z);
            archive(physics3DSettings.Substeps, physics3DSettings.EnableSleeping, physics3DSettings.EnableContinuousCollision,
                    physics3DSettings.Deterministic);
            UUID defaultMaterial3D;
            archive(defaultMaterial3D);
            physics3DSettings.DefaultMaterial = LoadAssetReference<PhysicsMaterial3D>(defaultMaterial3D);
            if (backend <= static_cast<uint32_t>(Physics3DBackendType::Bullet) &&
                Physics3D::IsBackendCompiled(static_cast<Physics3DBackendType>(backend)))
                physics3DSettings.Backend = static_cast<Physics3DBackendType>(backend);
            if (Physics3D::IsStartedUp())
                Physics3D::Get().SetSettings(physics3DSettings);

            stream->Close();
            return true;
        }
        catch (const std::exception& exception)
        {
            if (sceneMutationStarted)
            {
                m_Scene->m_Registry.clear();
                m_Scene->m_EntityMap.clear();
                delete m_Scene->m_RootEntity;
                m_Scene->m_RootEntity = nullptr;
            }
            if (m_Scene->m_RootEntity == nullptr)
                m_Scene->CreateRootEntity();
            CW_ENGINE_ERROR("Error deserializing binary scene \"{0}\". {1}.", filepath, std::string(exception.what()));
            stream->Close();
            return false;
        }
    }
} // namespace Crowny
