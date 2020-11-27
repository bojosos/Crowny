#include "cwpch.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Uuid.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneSerializer.h"

#include <yaml-cpp/yaml.h>

namespace YAML
{
    template<>
    struct convert<Crowny::Uuid>
    {
        static Node encode(const Crowny::Uuid& uuid)
        {
            Node node;
            node = uuid.ToString();
            return node;
        }

        static bool decode(const Node& node, Crowny::Uuid& rhs)
        {
            if (!node.IsScalar())
                return false;
            rhs = Crowny::Uuid(node.as<std::string>());

            return true;
        }
    };

    template<>
    struct convert<glm::vec3>
    {
        static Node encode(const glm::vec3& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
                return false;
             rhs.x = node[0].as<float>();
             rhs.y = node[1].as<float>();
             rhs.z = node[2].as<float>();

            return true;
        }
    };

    template<>
    struct convert<glm::vec4>
    {
        static Node encode(const glm::vec4& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);

            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
                return false;
             rhs.x = node[0].as<float>();
             rhs.y = node[1].as<float>();
             rhs.z = node[2].as<float>();
             rhs.w = node[3].as<float>();
             
            return true;
        }
    };
}

namespace Crowny
{

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

    YAML::Emitter& operator<<(YAML::Emitter& out, const Uuid& uuid)
    {
        out << uuid.ToString();
        return out;
    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene)
    {

    }

    static void SerializeEntity(YAML::Emitter& out, const Uuid& uuid, Entity entity)
    {
        if  (!entity)
            return;
        out << YAML::BeginMap; // Entity
        out << YAML::Key << "Entity" << YAML::Value << uuid;

        if (entity.HasComponent<TagComponent>())
        {
            out << YAML::Key << "TagComponent";
            out << YAML::BeginMap;
            const std::string& tag = entity.GetComponent<TagComponent>().Tag;
            out << YAML::Key << "Tag" << YAML::Value << tag;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MonoScriptComponent>())
        {
            out << YAML::Key << "MonoScriptComponent";
            out << YAML::BeginMap;
            const std::string& name = entity.GetComponent<MonoScriptComponent>().Name;
            out << YAML::Key << "Name" << YAML::Value << name;
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
            out << YAML::Key << "Uuid" << YAML::Value << UuidGenerator::Generate();
            out << YAML::EndMap;
        }

        if (entity.HasComponent<RelationshipComponent>())
        {
            out << YAML::Key << "RelationshipComponent";
            out << YAML::BeginMap;
            const auto& rc = entity.GetComponent<RelationshipComponent>();
            out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
            
            for (Entity e : rc.Children)
            {
                out << (uint32_t)e.GetHandle();
            }

            out << YAML::EndSeq << YAML::EndMap;
        }
        
        out << YAML::EndMap; // Entity
    }

    void SceneSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");

        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        for (auto kv : *(m_Scene->m_Entities))
        {
            SerializeEntity(out, kv.first, kv.second);
        };

        out << YAML::EndSeq << YAML::EndMap;
        m_Scene->m_Filepath = filepath;
        VirtualFileSystem::Get()->WriteTextFile(filepath, out.c_str());
    }

    void SceneSerializer::SerializeBinary(const std::string& filepath)
    {
        // Aaah hard
    }

    void SceneSerializer::Deserialize(const std::string& filepath)
    {
        std::string text = VirtualFileSystem::Get()->ReadTextFile(filepath);
        YAML::Node data = YAML::Load(text);
        if (!data["Scene"])
            return;

        std::unordered_map<Entity, YAML::Node> serializedComponents;
        std::string sceneName = data["Scene"].as<std::string>();
        m_Scene->m_Name = sceneName;
        m_Scene->m_Filepath = filepath;
        
        YAML::Node entities = data["Entities"];
        if (entities)
        {
            for (YAML::Node entity : entities)
            {
                Uuid id = entity["Entity"].as<Uuid>();

                std::string tag;
                YAML::Node tc = entity["TagComponent"];
                if (tc)
                    tag = tc["Tag"].as<std::string>();

                Entity deserialized = m_Scene->CreateEntity(id, tag);
                m_Scene->m_RootEntity->AddChild(deserialized);
                
                YAML::Node transform = entity["TransformComponent"];
                if (transform)
                {
                    auto& tc = deserialized.GetComponent<TransformComponent>();
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
                    auto& sc = deserialized.AddComponent<MonoScriptComponent>();
                    sc.Name = script["Name"].as<std::string>();
                }

                YAML::Node text = entity["TextComponent"];
                if (text)
                {
                    auto& tc = deserialized.AddComponent<TextComponent>();
                    tc.Text = text["Text"].as<std::string>();
                    tc.Font = CreateRef<Font>(text["Font"].as<std::string>(), "Deserialized font", 16);
                    tc.Color = text["Color"].as<glm::vec4>();
                }

                YAML::Node mesh = entity["MeshRendererComponent"];
                if (mesh)
                {
                    auto& mc = deserialized.AddComponent<MeshRendererComponent>();
                }

                YAML::Node rel = entity["RelationshipComponent"];
                if (rel)
                {
                    serializedComponents[deserialized] = rel;
                }
            }

            for (auto rc : serializedComponents)
            {
                auto& rl = rc.first.AddComponent<RelationshipComponent>();
                YAML::Node children = rc.second["Children"];
                for (auto child : children)
                {
                    Entity e = m_Scene->GetEntity(child.as<Uuid>());
                    e.SetParent(rc.first);
                    rl.Children.push_back(e);
                }
            }
        }
    }

    void SceneSerializer::DeserializeBinary(const std::string& filepath)
    {
        
    }

}