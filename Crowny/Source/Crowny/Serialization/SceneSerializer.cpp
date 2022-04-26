#include "cwpch.h"

#include "Crowny/Serialization/SceneSerializer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Yaml.h"

#include "Crowny/Ecs/Components.h"

namespace Crowny
{
    SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        const UUID& uuid = entity.GetUuid();
        if (!entity)
            return;
        out << YAML::BeginMap; // Entity
        out << YAML::Key << "Entity" << YAML::Value << uuid;

        if (entity.HasComponent<TagComponent>())
        {
            out << YAML::Key << "TagComponent";
            out << YAML::BeginMap;
            const String& tag = entity.GetComponent<TagComponent>().Tag;
            out << YAML::Key << "Tag" << YAML::Value << tag;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MonoScriptComponent>())
        {
            out << YAML::Key << "MonoScriptComponent";
            out << YAML::BeginMap;
            auto msc = entity.GetComponent<MonoScriptComponent>();
            const String& name = msc.GetTypeName();
            // auto& fields = msc.GetSerializableFields();
            // if (fields.size() > 0)
            // {
            //     out << YAML::Key << "Fields";
            //     for (auto* field : fields)
            //     {
            //         out << YAML::BeginMap;
            //         out << YAML::Key << field->GetName();
            //         out << YAML::Value << 5;
            //     }
            //     out << YAML::EndMap;
            // }

            // auto& props = msc.GetSerializableProperties();
            // if (props.size() > 0)
            // {
            //     out << YAML::Key << "Properties";
            //     for (auto* prop : props)
            //     {
            //         out << YAML::BeginMap;
            //         out << YAML::Key << prop->GetName();
            //         out << YAML::Value << 5;
            //     }
            //     out << YAML::EndMap;
            // }
            out << YAML::Key << "Name" << YAML::Value << name;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<AudioListenerComponent>())
            out << YAML::Key << "AudioListenerComponent" << YAML::Value << "";

        if (entity.HasComponent<AudioSourceComponent>())
        {
            out << YAML::Key << "AudioSourceComponent";
            out << YAML::BeginMap;
            const auto& asc = entity.GetComponent<AudioSourceComponent>();

            out << YAML::Key << "AudioClip" << YAML::Value << 111; // Some uuid here
            out << YAML::Key << "Volume" << YAML::Value << asc.GetVolume();
            out << YAML::Key << "Pitch" << YAML::Value << asc.GetPitch();
            out << YAML::Key << "Loop" << YAML::Value << asc.GetLooping();
            out << YAML::Key << "MinDistance" << YAML::Value << asc.GetMinDistance();
            out << YAML::Key << "MaxDistance" << YAML::Value << asc.GetMaxDistance();
            out << YAML::Key << "PlayOnAwake" << YAML::Value << asc.GetPlayOnAwake();
            out << YAML::Key << "Muted" << YAML::Value << asc.GetIsMuted();
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TextComponent>())
        {
            out << YAML::Key << "TextComponent";
            out << YAML::BeginMap;
            const auto& tc = entity.GetComponent<TextComponent>();
            out << YAML::Key << "Text" << YAML::Value << tc.Text;
            out << YAML::Key << "Font" << YAML::Value << tc.Font->GetFilepath().string(); // TODO: Do this better
            out << YAML::Key << "Color" << YAML::Value << tc.Color;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TransformComponent>())
        {
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            const auto& tc = entity.GetComponent<TransformComponent>();
            out << YAML::Key << "Position" << YAML::Value << tc.Position;
            out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
            out << YAML::Key << "Scale" << YAML::Value << tc.Scale;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<CameraComponent>())
        {
            out << YAML::Key << "CameraComponent";
            out << YAML::BeginMap;

            auto& camera = entity.GetComponent<CameraComponent>().Camera;
            out << YAML::Key << "ProjectionType" << YAML::Value << (uint32_t)camera.GetProjectionType();
            out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
            out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
            out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
            out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
            out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
            out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();

            out << YAML::Key << "HDR" << YAML::Value << camera.GetHDR();
            out << YAML::Key << "MSAA" << YAML::Value << camera.GetMSAA();
            out << YAML::Key << "OcclusionCulling" << camera.GetOcclusionCulling();
            out << YAML::Key << "BackgroundColor" << YAML::Value << camera.GetBackgroundColor();
            out << YAML::Key << "ViewportRect" << YAML::Value << camera.GetViewportRect();

            out << YAML::EndMap;
        }

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            out << YAML::Key << "SpriteRendererComponent";
            out << YAML::BeginMap;
            const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            out << YAML::Key << "Color" << YAML::Value << sprite.Color; // TODO: Save textures
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MeshRendererComponent>())
        {
            out << YAML::Key << "MeshRendererComponent";
            out << YAML::BeginMap;
            const auto& mesh = entity.GetComponent<MeshRendererComponent>();
            out << YAML::Key << "UUID" << YAML::Value << UuidGenerator::Generate();
            out << YAML::EndMap;
        }

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            out << YAML::Key << "Rigidbody2D";
            out << YAML::BeginMap;
            const auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
            out << YAML::Key << "BodyType" << YAML::Value << (uint32_t)rb2d.GetBodyType();
            out << YAML::Key << "Mass" << YAML::Value << rb2d.GetMass();
            out << YAML::Key << "GravityScale" << YAML::Value << rb2d.GetGravityScale();
            out << YAML::Key << "Constraints" << YAML::Value << (uint32_t)rb2d.GetConstraints();
            out << YAML::Key << "CollisionDetectionMode" << YAML::Value << rb2d.GetContinuousCollisionDetection();
            out << YAML::Key << "SleepMode" << YAML::Value << (uint32_t)rb2d.GetSleepMode();
            out << YAML::EndMap;
        }

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            out << YAML::Key << "BoxCollider2D";
            out << YAML::BeginMap;
            const auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
            out << YAML::Key << "Offset" << YAML::Value << bc2d.Offset;
            out << YAML::Key << "Size" << YAML::Value << bc2d.Size;
            out << YAML::Key << "IsTrigger" << YAML::Value << bc2d.IsTrigger;
            out << YAML::EndMap;
            // out << YAML::Key << "Material" << YAML::Value << bc2d.Material;
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            out << YAML::Key << "CircleCollider2D";
            out << YAML::BeginMap;
            const auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
            out << YAML::Key << "Offset" << YAML::Value << cc2d.Offset;
            out << YAML::Key << "Size" << YAML::Value << cc2d.Radius;
            out << YAML::Key << "IsTrigger" << YAML::Value << cc2d.IsTrigger;
            out << YAML::EndMap;
            // out << YAML::Key << "Material" << YAML::Value << cc2d.Material;
        }

        if (entity.HasComponent<RelationshipComponent>())
        {
            out << YAML::Key << "RelationshipComponent";
            out << YAML::BeginMap;
            const auto& rc = entity.GetComponent<RelationshipComponent>();
            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;

            for (Entity e : rc.Children)
            {
                out << e.GetUuid();
            }

            out << YAML::EndSeq << YAML::EndMap;
        }

        out << YAML::EndMap; // Entity
    }

    void SceneSerializer::Serialize(const Path& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");

        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        m_Scene->m_Registry.each([&](auto entityID) {
            Entity entity = { entityID, m_Scene.get() };
            SerializeEntity(out, entity);
        });

        out << YAML::EndSeq << YAML::EndMap;
        m_Scene->m_Filepath = filepath;
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        const char* str = out.c_str();
        stream->Write(str, std::strlen(str));
        stream->Close();
    }

    void SceneSerializer::SerializeBinary(const Path& filepath) {}

    void SceneSerializer::Deserialize(const Path& filepath)
    {
        String text = FileSystem::OpenFile(filepath)->GetAsString();
        YAML::Node data = YAML::Load(text);
        if (!data["Scene"])
            return;

        try
        {
            UnorderedMap<Entity, YAML::Node> serializedComponents;
            String sceneName = data["Scene"].as<String>();
            m_Scene->m_Name = sceneName;
            m_Scene->m_Filepath = filepath;
            // m_Scene->GetRootEntity().GetComponent<TagComponent>().Tag = sceneName;

            const YAML::Node& entities = data["Entities"];
            if (entities)
            {
                for (const YAML::Node& entity : entities)
                {
                    UUID id = entity["Entity"].as<UUID>();

                    String tag;
                    const YAML::Node& tc = entity["TagComponent"];
                    if (tc)
                        tag = tc["Tag"].as<String>();

                    Entity deserialized = m_Scene->CreateEntityWithUuid(id, tag);
                    // m_Scene->m_RootEntity->AddChild(deserialized);

                    const YAML::Node& transform = entity["TransformComponent"];
                    if (transform)
                    {
                        auto& tc = deserialized.GetComponent<TransformComponent>();
                        // tc.ComponentParent = deserialized;
                        tc.Position = transform["Position"].as<glm::vec3>();
                        tc.Rotation = transform["Rotation"].as<glm::vec3>();
                        tc.Scale = transform["Scale"].as<glm::vec3>();
                    }

                    const YAML::Node& camera = entity["CameraComponent"];
                    if (camera)
                    {
                        auto& cc = deserialized.AddComponent<CameraComponent>();
                        cc.Camera.SetProjectionType(
                          (SceneCamera::CameraProjection)camera["ProjectionType"].as<uint32_t>());
                        cc.Camera.SetPerspectiveVerticalFOV(camera["PerspectiveFOV"].as<float>());
                        cc.Camera.SetPerspectiveNearClip(camera["PerspectiveNear"].as<float>());
                        cc.Camera.SetPerspectiveFarClip(camera["PerspectiveFar"].as<float>());

                        cc.Camera.SetOrthographicSize(camera["OrthographicSize"].as<float>());
                        cc.Camera.SetOrthographicNearClip(camera["OrthographicNear"].as<float>());
                        cc.Camera.SetOrthographicFarClip(camera["OrthographicFar"].as<float>());

                        cc.Camera.SetHDR(camera["HDR"].as<bool>());
                        cc.Camera.SetMSAA(camera["MSAA"].as<bool>());
                        cc.Camera.SetOcclusionCulling(camera["OcclusionCulling"].as<bool>());

                        cc.Camera.SetBackgroundColor(camera["BackgroundColor"].as<glm::vec3>());
                        cc.Camera.SetViewportRect(camera["ViewportRect"].as<glm::vec4>());
                    }

                    const YAML::Node& sprite = entity["SpriteRendererComponent"];
                    if (sprite)
                    {
                        auto& tc = deserialized.AddComponent<SpriteRendererComponent>();
                        tc.Color = sprite["Color"].as<glm::vec4>();
                    }

                    const YAML::Node& script = entity["MonoScriptComponent"];
                    if (script)
                    {
                        auto& sc = deserialized.AddComponent<MonoScriptComponent>(script["Name"].as<String>());
                        // sc.ComponentParent = deserialized;
                    }

                    const YAML::Node& text = entity["TextComponent"];
                    if (text)
                    {
                        auto& tc = deserialized.AddComponent<TextComponent>();
                        tc.Text = text["Text"].as<String>();
                        tc.Font = CreateRef<Font>(text["Font"].as<String>(), "Deserialized font", 16);
                        tc.Color = text["Color"].as<glm::vec4>();
                    }

                    const YAML::Node& mesh = entity["MeshRendererComponent"];
                    if (mesh)
                    {
                        // auto& mc = deserialized.AddComponent<MeshRendererComponent>();
                    }

                    const YAML::Node& alc = entity["AudioListenerComponent"];
                    if (alc)
                    {
                        deserialized.AddComponent<AudioListenerComponent>();
                    }

                    const YAML::Node& source = entity["AudioSourceComponent"];
                    if (source)
                    {
                        auto& asc = deserialized.AddComponent<AudioSourceComponent>();
                        asc.SetPlayOnAwake(source["PlayOnAwake"].as<bool>());
                        // asc.SetAudioClip(source["AudioClip"].as<UUID>());
                        asc.SetVolume(source["Volume"].as<float>());
                        asc.SetPitch(source["Pitch"].as<float>());
                        asc.SetMinDistance(source["MinDistance"].as<float>());
                        asc.SetMaxDistance(source["MaxDistance"].as<float>());
                        asc.SetLooping(source["Loop"].as<bool>());
                    }

                    const YAML::Node& rb2d = entity["Rigidbody2D"];
                    if (rb2d)
                    {
                        auto& rb2dc = deserialized.AddComponent<Rigidbody2DComponent>();
                        rb2dc.SetBodyType((RigidbodyBodyType)rb2d["BodyType"].as<uint32_t>());
                        rb2dc.SetMass(rb2d["Mass"].as<float>());
                        rb2dc.SetGravityScale(rb2d["GravityScale"].as<float>());
                        if (const YAML::Node& collisionDetectionMode = entity["CollisionDetectionMode"])
                            rb2dc.SetContinuousCollisionDetection(collisionDetectionMode.as<String>() == "Continuous");
                        if (const YAML::Node& sleepMode = entity["SleepMode"])
                            rb2dc.SetSleepMode((RigidbodySleepMode)sleepMode.as<uint32_t>());
                        rb2dc.SetConstraints((Rigidbody2DConstraints)rb2d["Constraints"].as<uint32_t>());
                    }

                    const YAML::Node& bc2d = entity["BoxCollider2D"];
                    if (bc2d)
                    {
                        auto& bc2dc = deserialized.AddComponent<BoxCollider2DComponent>();
                        bc2dc.Offset = bc2d["Offset"].as<glm::vec2>();
                        bc2dc.Size = bc2d["Size"].as<glm::vec2>();
                        bc2dc.IsTrigger = bc2d["IsTrigger"].as<bool>();
                    }

                    const YAML::Node& cc2d = entity["CircleCollider2D"];
                    if (cc2d)
                    {
                        auto& cc2dc = deserialized.AddComponent<CircleCollider2DComponent>();
                        cc2dc.Offset = cc2d["Offset"].as<glm::vec2>();
                        cc2dc.Radius = cc2d["Size"].as<float>();
                        cc2dc.IsTrigger = cc2d["IsTrigger"].as<bool>();
                    }

                    const YAML::Node& rel = entity["RelationshipComponent"];
                    if (rel)
                        serializedComponents[deserialized] = rel;
                }

                for (auto [entity, node] : serializedComponents)
                {
                    auto& rl = entity.GetComponent<RelationshipComponent>();
                    const YAML::Node& children = node["Children"];
                    for (const auto& child : children)
                    {
                        Entity e = m_Scene->GetEntityFromUuid(child.as<UUID>());
                        e.SetParent(entity);
                    }
                }
            }
            auto view = m_Scene->GetAllEntitiesWith<TagComponent>();
            Entity root;
            for (auto e : view)
            {
                Entity entity = { e, m_Scene.get() };
                if (!entity.GetParent())
                    root = entity;
            }
            if (root)
                m_Scene->m_RootEntity = new Entity(root.GetHandle(), m_Scene.get());
        }
        catch (const std::exception& ex)
        {
            CW_ENGINE_ERROR("Error deserializing scene \"{0}\". {1}.", filepath, std::string(ex.what()));
        }
    }

    void SceneSerializer::DeserializeBinary(const Path& filepath) {}

} // namespace Crowny
