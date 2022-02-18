#include "cwpch.h"

#include "Crowny/Scene/SceneSerializer.h"

#include "Crowny/Common/Uuid.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const UUID& uuid)
    {
        out << uuid.ToString();
        return out;
    }

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
            const String& name = msc.GetManagedClass()->GetName();
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
            out << YAML::Key << "AudioListenerComponent";

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
            out << YAML::Key << "Font" << YAML::Value << tc.Font->GetFilepath(); // TODO: Do this better
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
            out << YAML::Key << "ProjectionType" << YAML::Value << (int32_t)camera.GetProjectionType();
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

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            out << YAML::Key << "BoxCollider2D";
            out << YAML::BeginMap;
            const auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
            out << YAML::Key << "Offset" << YAML::Value << bc2d.Offset;
            out << YAML::Key << "Size" << YAML::Value << bc2d.Size;
            out << YAML::Key << "IsTrigger" << YAML::Value << bc2d.IsTrigger;
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
            if (!entity)
                return;
            SerializeEntity(out, entity);
        });

        out << YAML::EndSeq << YAML::EndMap;
        m_Scene->m_Filepath = filepath;
        VirtualFileSystem::Get()->WriteTextFile(filepath, out.c_str());
    }

    void SceneSerializer::SerializeBinary(const Path& filepath) {}

    void SceneSerializer::Deserialize(const Path& filepath)
    {
        String text = VirtualFileSystem::Get()->ReadTextFile(filepath);
        YAML::Node data = YAML::Load(text);
        if (!data["Scene"])
            return;

        UnorderedMap<Entity, YAML::Node> serializedComponents;
        String sceneName = data["Scene"].as<String>();
        m_Scene->m_Name = sceneName;
        m_Scene->m_Filepath = filepath;

        YAML::Node entities = data["Entities"];
        if (entities)
        {
            for (YAML::Node entity : entities)
            {
                UUID id = entity["Entity"].as<UUID>();

                String tag;
                YAML::Node tc = entity["TagComponent"];
                if (tc)
                    tag = tc["Tag"].as<String>();

                Entity deserialized = m_Scene->CreateEntityWithUuid(id, tag);
                m_Scene->m_RootEntity->AddChild(deserialized);

                YAML::Node transform = entity["TransformComponent"];
                if (transform)
                {
                    auto& tc = deserialized.GetComponent<TransformComponent>();
                    // tc.ComponentParent = deserialized;
                    tc.Position = transform["Position"].as<glm::vec3>();
                    tc.Rotation = transform["Rotation"].as<glm::vec3>();
                    tc.Scale = transform["Scale"].as<glm::vec3>();
                }

                YAML::Node camera = entity["CameraComponent"];
                if (camera)
                {
                    auto& cc = deserialized.AddComponent<CameraComponent>();
                    cc.Camera.SetProjectionType((SceneCamera::CameraProjection)camera["ProjectionType"].as<int>());
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

                YAML::Node sprite = entity["SpriteRendererComponent"];
                if (sprite)
                {
                    auto& tc = deserialized.AddComponent<SpriteRendererComponent>();
                    tc.Color = sprite["Color"].as<glm::vec4>();
                }

                YAML::Node script = entity["MonoScriptComponent"];
                if (script)
                {
                    auto& sc = deserialized.AddComponent<MonoScriptComponent>(script["Name"].as<String>());
                    // sc.ComponentParent = deserialized;
                }

                YAML::Node text = entity["TextComponent"];
                if (text)
                {
                    auto& tc = deserialized.AddComponent<TextComponent>();
                    tc.Text = text["Text"].as<String>();
                    tc.Font = CreateRef<Font>(text["Font"].as<String>(), "Deserialized font", 16);
                    tc.Color = text["Color"].as<glm::vec4>();
                }

                YAML::Node mesh = entity["MeshRendererComponent"];
                if (mesh)
                {
                    // auto& mc = deserialized.AddComponent<MeshRendererComponent>();
                }

                YAML::Node alc = entity["AudioListenerComponent"];
                if (alc)
                {
                    deserialized.AddComponent<AudioListenerComponent>();
                }

                YAML::Node source = entity["AudioSourceComponent"];
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

                YAML::Node rel = entity["RelationshipComponent"];
                if (rel)
                {
                    serializedComponents[deserialized] = rel;
                }
            }

            for (auto rc : serializedComponents)
            {
                auto& rl = rc.first.GetComponent<RelationshipComponent>();
                YAML::Node children = rc.second["Children"];
                for (auto child : children)
                {
                    Entity e = m_Scene->GetEntityFromUuid(child.as<UUID>());
                    e.SetParent(rc.first);
                }
            }
        }
    }

    void SceneSerializer::DeserializeBinary(const Path& filepath) {}

} // namespace Crowny
