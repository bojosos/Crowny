#include "cwpch.h"

#include "Crowny/Serialization/SceneSerializer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Renderer/Font.h"

#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SettingsSerializer.h"

#include "Crowny/Ecs/Components.h"

#include <cereal/types/string.hpp>

namespace Crowny
{
    static constexpr uint32_t SceneVersion = 0;

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

    void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        const UUID& uuid = entity.GetUuid();
        if (!entity)
            return;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Entity", uuid);

        if (entity.HasComponent<TagComponent>())
        {
            BeginYAMLMap(out, "TagComponent");

            SerializeValueYAML(out, "Tag", entity.GetName());

            EndYAMLMap(out, "TagComponent");
        }

        if (entity.HasComponent<MonoScriptComponent>())
        {
            const auto& msc = entity.GetComponent<MonoScriptComponent>();
            if (msc.Scripts.size() > 0)
            {
                BeginYAMLMap(out, "MonoScriptComponent");

                for (const auto& script : msc.Scripts)
                {
                    SerializeValueYAML(out, script.GetTypeName().c_str(), YAML::BeginSeq);

                    Ref<SerializableObject> serializableObject = SerializableObject::CreateFromMonoObject(script.GetManagedInstance());
                    serializableObject->SerializeYAML(out);

                    EndYAMLSeq(out);
                }
                EndYAMLMap(out, "MonoScriptComponent");
            }
        }

        if (entity.HasComponent<AudioListenerComponent>())
            SerializeValueYAML(out, "AudioListenerComponent", YAML::Null);

        if (entity.HasComponent<AudioSourceComponent>())
        {
            const auto& asc = entity.GetComponent<AudioSourceComponent>();
            BeginYAMLMap(out, "AudioSourceComponent");

            SerializeValueYAML(out, "AudioClip", asc.GetClip().GetUUID());
            SerializeValueYAML(out, "Volume", asc.GetVolume());
            SerializeValueYAML(out, "Pitch", asc.GetPitch());
            SerializeValueYAML(out, "Loop", asc.GetLooping());
            SerializeValueYAML(out, "MinDistance", asc.GetMinDistance());
            SerializeValueYAML(out, "MaxDistance", asc.GetMaxDistance());
            SerializeValueYAML(out, "PlayOnAwake", asc.GetPlayOnAwake());
            SerializeValueYAML(out, "Muted", asc.GetIsMuted());

            EndYAMLMap(out, "AudioSourceComponent");
        }

        if (entity.HasComponent<TextComponent>())
        {
            const auto& tc = entity.GetComponent<TextComponent>();
            BeginYAMLMap(out, "TextComponent");

            SerializeValueYAML(out, "Text", tc.Text);
            SerializeValueYAML(out, "Font", tc.Font.GetUUID());
            SerializeValueYAML(out, "Color", tc.Color);
            SerializeValueYAML(out, "Size", tc.Size);
            SerializeValueYAML(out, "AutoSize", tc.AutoSize);
            SerializeValueYAML(out, "Wrapping", tc.Wrapping);
            SerializeValueYAML(out, "OutlineColor", tc.OutlineColor);
            SerializeValueYAML(out, "Thickness", tc.Thickness);
            SerializeValueYAML(out, "CharacterSpacing", tc.CharacterSpacing);
            SerializeValueYAML(out, "WordSpacing", tc.WordSpacing);
            SerializeValueYAML(out, "LineSpacing", tc.LineSpacing);
            SerializeValueYAML(out, "UseKerning", tc.UseKerning);
            SerializeValueYAML(out, "FontStyle", (uint32_t)tc.FontStyle);
            SerializeEnumYAML(out, "Overflow", tc.Overflow);
            SerializeEnumYAML(out, "HorizontalAlignment", tc.HorizontalAlignment);
            SerializeEnumYAML(out, "VerticalAlignment", tc.VerticalAlignment);

            EndYAMLMap(out, "TextComponent");
        }

        if (entity.HasComponent<TransformComponent>())
        {
            BeginYAMLMap(out, "TransformComponent");

            SerializeValueYAML(out, "Position", entity.GetLocalPosition());
            SerializeValueYAML(out, "Rotation", entity.GetLocalRotation());
            SerializeValueYAML(out, "Scale", entity.GetLocalScale());

            EndYAMLMap(out, "TransformComponent");
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& camera = entity.GetComponent<CameraComponent>().Camera;
            BeginYAMLMap(out, "CameraComponent");

            SerializeEnumYAML(out, "ProjectionType", camera.GetProjectionType());
            SerializeValueYAML(out, "PerspectiveFOV", camera.GetPerspectiveVerticalFOV());
            SerializeValueYAML(out, "PerspectiveNear", camera.GetPerspectiveNearClip());
            SerializeValueYAML(out, "PerspectiveFar", camera.GetPerspectiveFarClip());
            SerializeValueYAML(out, "OrthographicSize", camera.GetOrthographicSize());
            SerializeValueYAML(out, "OrthographicNear", camera.GetOrthographicNearClip());
            SerializeValueYAML(out, "OrthographicFar", camera.GetOrthographicFarClip());
            SerializeValueYAML(out, "HDR", camera.GetHDR());
            SerializeValueYAML(out, "MSAA", camera.GetMSAA());
            SerializeValueYAML(out, "OcclusionCulling", camera.GetOcclusionCulling());
            SerializeValueYAML(out, "BackgroundColor", camera.GetBackgroundColor());
            SerializeValueYAML(out, "ViewportRect", camera.GetViewportRect());

            EndYAMLMap(out, "CameraComponent");
        }

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            BeginYAMLMap(out, "SpriteRendererComponent");

            SerializeValueYAML(out, "Color", sprite.Color);
            SerializeValueYAML(out, "Texture", sprite.Texture.GetUUID());

            EndYAMLMap(out, "SpriteRendererComponent");
        }

        if (entity.HasComponent<MeshRendererComponent>())
        {
            const MeshRendererComponent& mesh = entity.GetComponent<MeshRendererComponent>();
            BeginYAMLMap(out, "MeshRendererComponent");

            SerializeValueYAML(out, "Mesh", mesh.MeshHandle.GetUUID());

            EndYAMLMap(out, "MeshRendererComponent");
        }

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            const auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
            BeginYAMLMap(out, "Rigidbody2DComponent");

            SerializeEnumYAML(out, "BodyType", rb2d.GetBodyType());
            SerializeValueYAML(out, "Mass", rb2d.GetMass());
            SerializeValueYAML(out, "GravityScale", rb2d.GetGravityScale());
            SerializeFlagsYAML(out, "Constraints", rb2d.GetConstraints());
            SerializeEnumYAML(out, "CollisionDetectionMode", rb2d.GetCollisionDetectionMode());
            SerializeEnumYAML(out, "SleepMode", rb2d.GetSleepMode());
            SerializeValueYAML(out, "LinearDrag", rb2d.GetLinearDrag());
            SerializeValueYAML(out, "AngularDrag", rb2d.GetAngularDrag());
            SerializeValueYAML(out, "LayerMask", rb2d.GetLayerMask());
            SerializeValueYAML(out, "AutoMass", rb2d.GetAutoMass());
            SerializeEnumYAML(out, "Interpolation", rb2d.GetInterpolationMode());

            EndYAMLMap(out, "Rigidbody2DComponent");
        }

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
            BeginYAMLMap(out, "BoxCollider2DComponent");

            SerializeValueYAML(out, "Offset", bc2d.GetOffset());
            SerializeValueYAML(out, "Size", bc2d.GetSize());
            SerializeValueYAML(out, "IsTrigger", bc2d.IsTrigger());
            if (bc2d.GetMaterial().GetUUID() != Physics2D::Get().GetDefaultMaterial().GetUUID()) // TODO: fix this. shouldn't need to check anything
                SerializeValueYAML(out, "Material", bc2d.GetMaterial().GetUUID());

            EndYAMLMap(out, "BoxCollider2DComponent");
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
            BeginYAMLMap(out, "CircleCollider2DComponent");

            SerializeValueYAML(out, "Offset", cc2d.GetOffset());
            SerializeValueYAML(out, "Size", cc2d.GetRadius());
            SerializeValueYAML(out, "IsTrigger", cc2d.IsTrigger());
            if (cc2d.GetMaterial().GetUUID() != Physics2D::Get().GetDefaultMaterial().GetUUID())
                SerializeValueYAML(out, "Material", cc2d.GetMaterial().GetUUID());

            EndYAMLMap(out, "CircleCollider2DComponent");
        }

        if (entity.HasComponent<RelationshipComponent>())
        {
            const auto& rc = entity.GetComponent<RelationshipComponent>();
            BeginYAMLMap(out, "RelationshipComponent");

            SerializeValueYAML(out, "Children", YAML::BeginSeq);

            for (Entity e : rc.Children)
                out << e.GetUuid();

            out << YAML::EndSeq;

            EndYAMLMap(out, "RelationshipComponent");
        }

        EndYAMLMap(out, "Entity");
    }

    void SceneSerializer::Serialize(const Path& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");

        out << YAML::BeginMap;

        SerializeValueYAML(out, "Version", SceneVersion);
        SerializeValueYAML(out, "Scene", m_Scene->GetName());

        SerializeValueYAML(out, "Entities", YAML::BeginSeq);
        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
        m_Scene->m_Registry.each([&](auto entityID) {
            Entity entity = { entityID, m_Scene.get() };
            SerializeEntity(out, entity);
        });
        out << YAML::EndSeq;

        TimeSettingsSerializer::Serialize(Application::Get().GetTimeSettings(), out);
        Physics2DSettingsSerializer::Serialize(Physics2D::Get().GetPhysicsSettings(), out);

        out << YAML::EndMap;

        m_Scene->m_Filepath = filepath;
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        const char* str = out.c_str();
        stream->Write(str, std::strlen(str));
        stream->Close();
    }

    enum class BinaryComponentType : uint32_t
    {
        Tag = 0,
        Transform,
        Camera,
        SpriteRenderer,
        MeshRenderer,
        Text,
        AudioListener,
        AudioSource,
        MonoScript,
        Rigidbody2D,
        BoxCollider2D,
        CircleCollider2D,
        Relationship
    };

    void SceneSerializer::SerializeBinary(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        BinaryDataStreamOutputArchive archive(stream);

        uint32_t version = SceneVersion;
        archive(version);
        String sceneName = m_Scene->GetName();
        archive(sceneName);

        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });

        // Count entities
        uint32_t entityCount = 0;
        m_Scene->m_Registry.each([&](auto entityID) { entityCount++; });
        archive(entityCount);

        m_Scene->m_Registry.each([&](auto entityID) {
            Entity entity = { entityID, m_Scene.get() };
            if (!entity)
                return;

            UUID uuid = entity.GetUuid();
            archive(uuid);
            String entityName = entity.GetName();
            archive(entityName);

            // Count components for this entity
            uint32_t componentCount = 0;
            if (entity.HasComponent<TagComponent>()) componentCount++;
            if (entity.HasComponent<TransformComponent>()) componentCount++;
            if (entity.HasComponent<CameraComponent>()) componentCount++;
            if (entity.HasComponent<SpriteRendererComponent>()) componentCount++;
            if (entity.HasComponent<MeshRendererComponent>()) componentCount++;
            if (entity.HasComponent<TextComponent>()) componentCount++;
            if (entity.HasComponent<AudioListenerComponent>()) componentCount++;
            if (entity.HasComponent<AudioSourceComponent>()) componentCount++;
            if (entity.HasComponent<MonoScriptComponent>())
            {
                const auto& msc = entity.GetComponent<MonoScriptComponent>();
                if (msc.Scripts.size() > 0) componentCount++;
            }
            if (entity.HasComponent<Rigidbody2DComponent>()) componentCount++;
            if (entity.HasComponent<BoxCollider2DComponent>()) componentCount++;
            if (entity.HasComponent<CircleCollider2DComponent>()) componentCount++;
            if (entity.HasComponent<RelationshipComponent>()) componentCount++;
            archive(componentCount);

            // TagComponent
            if (entity.HasComponent<TagComponent>())
            {
                archive((uint32_t)BinaryComponentType::Tag);
                String tag = entity.GetName();
                archive(tag);
            }

            // TransformComponent
            if (entity.HasComponent<TransformComponent>())
            {
                archive((uint32_t)BinaryComponentType::Transform);
                glm::vec3 pos = entity.GetLocalPosition();
                glm::quat rot = entity.GetLocalRotation();
                glm::vec3 scale = entity.GetLocalScale();
                archive(pos.x, pos.y, pos.z);
                archive(rot.x, rot.y, rot.z, rot.w);
                archive(scale.x, scale.y, scale.z);
            }

            // CameraComponent
            if (entity.HasComponent<CameraComponent>())
            {
                const auto& camera = entity.GetComponent<CameraComponent>().Camera;
                archive((uint32_t)BinaryComponentType::Camera);
                archive((uint32_t)camera.GetProjectionType());
                archive(camera.GetPerspectiveVerticalFOV());
                archive(camera.GetPerspectiveNearClip());
                archive(camera.GetPerspectiveFarClip());
                archive(camera.GetOrthographicSize());
                archive(camera.GetOrthographicNearClip());
                archive(camera.GetOrthographicFarClip());
                archive(camera.GetHDR());
                archive(camera.GetMSAA());
                archive(camera.GetOcclusionCulling());
                glm::vec3 bgColor = camera.GetBackgroundColor();
                archive(bgColor.x, bgColor.y, bgColor.z);
                glm::vec4 vpRect = camera.GetViewportRect();
                archive(vpRect.x, vpRect.y, vpRect.z, vpRect.w);
            }

            // SpriteRendererComponent
            if (entity.HasComponent<SpriteRendererComponent>())
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                archive((uint32_t)BinaryComponentType::SpriteRenderer);
                archive(sprite.Color.x, sprite.Color.y, sprite.Color.z, sprite.Color.w);
                UUID texUuid = sprite.Texture.GetUUID();
                archive(texUuid);
            }

            // MeshRendererComponent
            if (entity.HasComponent<MeshRendererComponent>())
            {
                const auto& mesh = entity.GetComponent<MeshRendererComponent>();
                archive((uint32_t)BinaryComponentType::MeshRenderer);
                UUID meshUuid = mesh.MeshHandle.GetUUID();
                archive(meshUuid);
            }

            // TextComponent
            if (entity.HasComponent<TextComponent>())
            {
                const auto& tc = entity.GetComponent<TextComponent>();
                archive((uint32_t)BinaryComponentType::Text);
                archive(tc.Text);
                UUID fontUuid = tc.Font.GetUUID();
                archive(fontUuid);
                archive(tc.Color.x, tc.Color.y, tc.Color.z, tc.Color.w);
                archive(tc.Size);
                archive(tc.AutoSize);
                archive(tc.Wrapping);
                archive(tc.OutlineColor.x, tc.OutlineColor.y, tc.OutlineColor.z, tc.OutlineColor.w);
                archive(tc.Thickness);
                archive(tc.CharacterSpacing);
                archive(tc.WordSpacing);
                archive(tc.LineSpacing);
                archive(tc.UseKerning);
                archive((uint32_t)tc.FontStyle);
                archive((uint32_t)tc.Overflow);
                archive((uint32_t)tc.HorizontalAlignment);
                archive((uint32_t)tc.VerticalAlignment);
            }

            // AudioListenerComponent
            if (entity.HasComponent<AudioListenerComponent>())
            {
                archive((uint32_t)BinaryComponentType::AudioListener);
            }

            // AudioSourceComponent
            if (entity.HasComponent<AudioSourceComponent>())
            {
                const auto& asc = entity.GetComponent<AudioSourceComponent>();
                archive((uint32_t)BinaryComponentType::AudioSource);
                UUID clipUuid = asc.GetClip().GetUUID();
                archive(clipUuid);
                archive(asc.GetVolume());
                archive(asc.GetPitch());
                archive(asc.GetLooping());
                archive(asc.GetMinDistance());
                archive(asc.GetMaxDistance());
                archive(asc.GetPlayOnAwake());
                archive(asc.GetIsMuted());
            }

            // MonoScriptComponent
            if (entity.HasComponent<MonoScriptComponent>())
            {
                const auto& msc = entity.GetComponent<MonoScriptComponent>();
                if (msc.Scripts.size() > 0)
                {
                    archive((uint32_t)BinaryComponentType::MonoScript);
                    uint32_t scriptCount = (uint32_t)msc.Scripts.size();
                    archive(scriptCount);
                    for (const auto& script : msc.Scripts)
                    {
                        String typeName = script.GetTypeName();
                        archive(typeName);
                        Ref<SerializableObject> serializableObject = SerializableObject::CreateFromMonoObject(script.GetManagedInstance());
                        Save(archive, *serializableObject);
                    }
                }
            }

            // Rigidbody2DComponent
            if (entity.HasComponent<Rigidbody2DComponent>())
            {
                const auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
                archive((uint32_t)BinaryComponentType::Rigidbody2D);
                archive((uint32_t)rb2d.GetBodyType());
                archive(rb2d.GetMass());
                archive(rb2d.GetGravityScale());
                archive((uint32_t)rb2d.GetConstraints());
                archive((uint32_t)rb2d.GetCollisionDetectionMode());
                archive((uint32_t)rb2d.GetSleepMode());
                archive(rb2d.GetLinearDrag());
                archive(rb2d.GetAngularDrag());
                archive(rb2d.GetLayerMask());
                archive(rb2d.GetAutoMass());
                archive((uint32_t)rb2d.GetInterpolationMode());
            }

            // BoxCollider2DComponent
            if (entity.HasComponent<BoxCollider2DComponent>())
            {
                const auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
                archive((uint32_t)BinaryComponentType::BoxCollider2D);
                archive(bc2d.GetOffset().x, bc2d.GetOffset().y);
                archive(bc2d.GetSize().x, bc2d.GetSize().y);
                archive(bc2d.IsTrigger());
                UUID matUuid = bc2d.GetMaterial().GetUUID();
                archive(matUuid);
            }

            // CircleCollider2DComponent
            if (entity.HasComponent<CircleCollider2DComponent>())
            {
                const auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
                archive((uint32_t)BinaryComponentType::CircleCollider2D);
                archive(cc2d.GetOffset().x, cc2d.GetOffset().y);
                archive(cc2d.GetRadius());
                archive(cc2d.IsTrigger());
                UUID matUuid = cc2d.GetMaterial().GetUUID();
                archive(matUuid);
            }

            // RelationshipComponent
            if (entity.HasComponent<RelationshipComponent>())
            {
                const auto& rc = entity.GetComponent<RelationshipComponent>();
                archive((uint32_t)BinaryComponentType::Relationship);
                uint32_t childCount = (uint32_t)rc.Children.size();
                archive(childCount);
                for (Entity e : rc.Children)
                {
                    UUID childUuid = e.GetUuid();
                    archive(childUuid);
                }
            }
        });

        // Serialize time settings
        {
            const auto& ts = Application::Get().GetTimeSettings();
            archive(ts->TimeScale);
            archive(ts->MaxTimestep);
            archive(ts->FixedTimestep);
        }

        // Serialize physics settings
        {
            const auto& ps = Physics2D::Get().GetPhysicsSettings();
            archive(ps->Gravity.x, ps->Gravity.y);
            archive(ps->VelocityIterations);
            archive(ps->PositionIterations);
            for (uint32_t i = 0; i < 32; i++)
                archive(ps->LayerNames[i]);
            for (uint32_t i = 0; i < 32; i++)
                archive(ps->MaskBits[i]);
        }

        m_Scene->m_Filepath = filepath;
        stream->Close();
    }

    void SceneSerializer::Deserialize(const Path& filepath)
    {
        String text = FileSystem::OpenFile(filepath)->GetAsString();
        try
        {
            YAML::Node data = YAML::Load(text);
            const YAML::Node versionNode = data["Version"];
            if (!versionNode && SceneVersion != 0)
                CW_ENGINE_INFO("Missing scene version! Assuming version 0");
            const uint32_t version = versionNode ? versionNode.as<uint32_t>() : 0;
            if (version != SceneVersion)
                CW_ENGINE_INFO("Loading scene with version: {0}, current: {1}", version, SceneVersion);
            const YAML::Node sceneNode = data["Scene"];
            if (!sceneNode)
                return;

            m_Scene->m_Registry.clear();
            m_Scene->m_EntityMap.clear();
            delete m_Scene->m_RootEntity;
            m_Scene->m_RootEntity = nullptr;

            UnorderedMap<Entity, YAML::Node> serializedComponents;
            const String sceneName = sceneNode.as<String>();
            m_Scene->m_Name = sceneName;
            m_Scene->m_Filepath = filepath;

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

                    const YAML::Node& transform = entity["TransformComponent"];
                    if (transform)
                    {
                        auto& tc = deserialized.GetTransform();
                        tc.SetPosition(transform["Position"].as<glm::vec3>(glm::vec3()));
                        tc.SetRotation(transform["Rotation"].as<glm::quat>(glm::quat()));
                        tc.SetScale(transform["Scale"].as<glm::vec3>(glm::vec3()));
                    }

                    const YAML::Node& camera = entity["CameraComponent"];
                    if (camera)
                    {
                        auto& cc = deserialized.AddComponent<CameraComponent>();
                        cc.Camera.SetProjectionType((SceneCamera::CameraProjection)camera["ProjectionType"].as<uint32_t>());
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

                    const YAML::Node& text = entity["TextComponent"];
                    if (text)
                    {
                        TextComponent& tc = deserialized.AddComponent<TextComponent>();
                        tc.Text = text["Text"].as<String>();
                        tc.Font = LoadAssetHandle<Font>(text["Font"].as<UUID>(UUID::EMPTY));
                        tc.Color = text["Color"].as<glm::vec4>(glm::vec4(1.0f));
                        tc.Size = text["Size"].as<float>(0.0f);
                        tc.AutoSize = text["AutoSize"].as<bool>(false);
                        tc.Wrapping = text["Wrapping"].as<bool>(false);
                        tc.FontStyle = (TextFontStyleBits)text["FontStyle"].as<uint32_t>(0);
                        tc.OutlineColor = text["OutlineColor"].as<glm::vec4>(glm::vec4(0.0f));
                        tc.Thickness = text["Thickness"].as<float>(0.8f);
                        tc.CharacterSpacing = text["CharacterSpacing"].as<float>(0.0f);
                        tc.WordSpacing = text["WordSpacing"].as<float>(0.0f);
                        tc.LineSpacing = text["LineSpacing"].as<float>(0.0f);
                        tc.UseKerning = text["UseKerning"].as<bool>(true);
                        DeserializeEnumYAML(text, "Overflow", tc.Overflow, TextOverflow::Overflow);
                        DeserializeEnumYAML(text, "HorizontalAlignment", tc.HorizontalAlignment, TextHorizontalAlignment::Left);
                        DeserializeEnumYAML(text, "VerticalAlignment", tc.VerticalAlignment, TextVerticalAlignment::Top);
                    }

                    const YAML::Node& mesh = entity["MeshRendererComponent"];
                    if (mesh)
                    {
                        MeshRendererComponent& mc = deserialized.AddComponent<MeshRendererComponent>();
                        mc.MeshHandle = LoadAssetHandle<Mesh>(mesh["Mesh"].as<UUID>(UUID::EMPTY));
                        mc.BaseMaterial = nullptr;
                    }

                    const YAML::Node& alc = entity["AudioListenerComponent"];
                    if (alc)
                        deserialized.AddComponent<AudioListenerComponent>();

                    const YAML::Node& source = entity["AudioSourceComponent"];
                    if (source)
                    {
                        auto& asc = deserialized.AddComponent<AudioSourceComponent>();

                        asc.SetPlayOnAwake(source["PlayOnAwake"].as<bool>());
                        asc.SetVolume(source["Volume"].as<float>());
                        asc.SetPitch(source["Pitch"].as<float>());
                        asc.SetMinDistance(source["MinDistance"].as<float>());
                        asc.SetMaxDistance(source["MaxDistance"].as<float>());
                        asc.SetLooping(source["Loop"].as<bool>());
                        asc.SetIsMuted(source["Muted"].as<bool>(false));
                        asc.SetClip(LoadAssetHandle<AudioClip>(source["AudioClip"].as<UUID>(UUID::EMPTY)));
                    }

                    auto loadPhysicsMaterial = [&](const YAML::Node& node) {
                        const YAML::Node& material = node["Material"];
                        if (!material)
                            return Physics2D::Get().GetDefaultMaterial();
                        return LoadAssetHandle<PhysicsMaterial2D>(material.as<UUID>(UUID::EMPTY));
                    };

                    const YAML::Node& bc2d = entity["BoxCollider2D"];
                    if (bc2d)
                    {
                        auto& bc2dc = deserialized.AddComponent<BoxCollider2DComponent>();
                        bc2dc.SetOffset(bc2d["Offset"].as<glm::vec2>(), deserialized);
                        bc2dc.SetSize(bc2d["Size"].as<glm::vec2>(), deserialized);
                        bc2dc.SetIsTrigger(bc2d["IsTrigger"].as<bool>());
                        bc2dc.SetMaterial(loadPhysicsMaterial(bc2d));
                    }

                    const YAML::Node& cc2d = entity["CircleCollider2D"];
                    if (cc2d)
                    {
                        auto& cc2dc = deserialized.AddComponent<CircleCollider2DComponent>();
                        cc2dc.SetOffset(cc2d["Offset"].as<glm::vec2>(), deserialized);
                        cc2dc.SetRadius(cc2d["Size"].as<float>(), deserialized);
                        cc2dc.SetIsTrigger(cc2d["IsTrigger"].as<bool>());
                        cc2dc.SetMaterial(loadPhysicsMaterial(cc2d));
                    }

                    const YAML::Node& rb2d = entity["Rigidbody2D"];
                    if (rb2d)
                    {
                        auto& rb2dc = deserialized.AddComponent<Rigidbody2DComponent>();
                        rb2dc.SetBodyType((RigidbodyBodyType)rb2d["BodyType"].as<uint32_t>());
                        rb2dc.SetMass(rb2d["Mass"].as<float>());
                        rb2dc.SetGravityScale(rb2d["GravityScale"].as<float>());
                        rb2dc.SetLayerMask(rb2d["LayerMask"].as<uint32_t>(0), deserialized);
                        rb2dc.SetCollisionDetectionMode((CollisionDetectionMode2D)rb2d["CollisionDetectionMode"].as<uint32_t>(0));
                        rb2dc.SetSleepMode((RigidbodySleepMode)rb2d["SleepMode"].as<uint32_t>(1));
                        rb2dc.SetLinearDrag(rb2d["LinearDrag"].as<float>(0.0f));
                        rb2dc.SetAngularDrag(rb2d["AngularDrag"].as<float>(0.05f));
                        rb2dc.SetConstraints((Rigidbody2DConstraints)rb2d["Constraints"].as<uint32_t>());
                        rb2dc.SetAutoMass(rb2d["AutoMass"].as<bool>(false), deserialized);
                        rb2dc.SetInterpolationMode((RigidbodyInterpolation)rb2d["Interpolation"].as<uint32_t>());
                    }

                    // Keep last because of all of the RequireComponent magic.
                    const YAML::Node& script = entity["MonoScriptComponent"];
                    if (script)
                    {
                        auto& msc = deserialized.AddComponent<MonoScriptComponent>();
                        for (const auto& scriptNode : script)
                        {
                            Ref<SerializableObject> obj = SerializableObject::DeserializeYAML(scriptNode.second);

                            MonoClass* monoClass = MonoManager::Get().FindClass("Sandbox", scriptNode.first.as<String>());
                            CW_ENGINE_ASSERT(monoClass != nullptr);
                            ::MonoClass* rawClass = monoClass->GetInternalPtr();
                            MonoReflectionType* runtimeType = MonoUtils::GetType(rawClass);

                            msc.Scripts.push_back(MonoScript(runtimeType));
                            msc.Scripts.back().m_SerializedObjectData = obj;
                            msc.Scripts.back().Create(deserialized);
                        }
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
                        e.GetComponent<RelationshipComponent>().Parent = entity;
                        rl.Children.push_back(e);
                    }
                }
            }
            auto view = m_Scene->GetAllEntitiesWith<TagComponent>();
            Entity root;
            for (auto e : view)
            {
                Entity entity = { e, m_Scene.get() };
                if (!entity.GetParent())
                {
                    root = entity;
                    break;
                }
            }
            if (root)
                m_Scene->m_RootEntity = new Entity(root.GetHandle(), m_Scene.get());

            if (m_Scene->m_RootEntity)
                m_Scene->m_RootEntity->NotifyTransformChanged();

            const Ref<TimeSettings>& timeSettings = TimeSettingsSerializer::Deserialize(data);
            if (Application::IsStartedUp())
                Application::Get().SetTimeSettings(timeSettings);
            const Ref<Physics2DSettings>& physicsSettings = Physics2DSettingsSerializer::Deserialize(data);
            Physics2D::Get().SetPhysicsSettings(physicsSettings);
        }
        catch (const std::exception& ex)
        {
            if (!m_Scene->m_RootEntity)
                m_Scene->CreateRootEntity();
            CW_ENGINE_ERROR("Error deserializing scene \"{0}\". {1}.", filepath, std::string(ex.what()));
        }
    }

    void SceneSerializer::DeserializeBinary(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);

        try
        {
            uint32_t version;
            archive(version);
            if (version != SceneVersion)
                CW_ENGINE_INFO("Loading binary scene with version: {0}, current: {1}", version, SceneVersion);

            String sceneName;
            archive(sceneName);

            m_Scene->m_Registry.clear();
            m_Scene->m_EntityMap.clear();
            delete m_Scene->m_RootEntity;
            m_Scene->m_RootEntity = nullptr;

            m_Scene->m_Name = sceneName;
            m_Scene->m_Filepath = filepath;

            uint32_t entityCount;
            archive(entityCount);

            // Map to defer relationship reconstruction
            UnorderedMap<Entity, Vector<UUID>> serializedRelationships;

            for (uint32_t i = 0; i < entityCount; i++)
            {
                UUID uuid;
                archive(uuid);
                String name;
                archive(name);

                uint32_t componentCount;
                archive(componentCount);

                // We don't create the entity until we've read TagComponent or fall back to name
                Entity deserialized;

                // We need to read the tag first if present to pass it to CreateEntityWithUuid.
                // Since components are written in order, TagComponent is first if present.
                // We'll read components in order and handle them.
                for (uint32_t c = 0; c < componentCount; c++)
                {
                    uint32_t typeTag;
                    archive(typeTag);
                    BinaryComponentType componentType = (BinaryComponentType)typeTag;

                    switch (componentType)
                    {
                    case BinaryComponentType::Tag:
                    {
                        String tag;
                        archive(tag);
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, tag);
                        break;
                    }
                    case BinaryComponentType::Transform:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& tc = deserialized.GetTransform();
                        glm::vec3 pos;
                        glm::quat rot;
                        glm::vec3 scale;
                        archive(pos.x, pos.y, pos.z);
                        archive(rot.x, rot.y, rot.z, rot.w);
                        archive(scale.x, scale.y, scale.z);
                        tc.SetPosition(pos);
                        tc.SetRotation(rot);
                        tc.SetScale(scale);
                        break;
                    }
                    case BinaryComponentType::Camera:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& cc = deserialized.AddComponent<CameraComponent>();
                        uint32_t projType;
                        archive(projType);
                        cc.Camera.SetProjectionType((SceneCamera::CameraProjection)projType);
                        float perspFov, perspNear, perspFar;
                        archive(perspFov);
                        archive(perspNear);
                        archive(perspFar);
                        cc.Camera.SetPerspectiveVerticalFOV(perspFov);
                        cc.Camera.SetPerspectiveNearClip(perspNear);
                        cc.Camera.SetPerspectiveFarClip(perspFar);
                        float orthoSize, orthoNear, orthoFar;
                        archive(orthoSize);
                        archive(orthoNear);
                        archive(orthoFar);
                        cc.Camera.SetOrthographicSize(orthoSize);
                        cc.Camera.SetOrthographicNearClip(orthoNear);
                        cc.Camera.SetOrthographicFarClip(orthoFar);
                        bool hdr, msaa, occlusionCulling;
                        archive(hdr);
                        archive(msaa);
                        archive(occlusionCulling);
                        cc.Camera.SetHDR(hdr);
                        cc.Camera.SetMSAA(msaa);
                        cc.Camera.SetOcclusionCulling(occlusionCulling);
                        glm::vec3 bgColor;
                        archive(bgColor.x, bgColor.y, bgColor.z);
                        cc.Camera.SetBackgroundColor(bgColor);
                        glm::vec4 vpRect;
                        archive(vpRect.x, vpRect.y, vpRect.z, vpRect.w);
                        cc.Camera.SetViewportRect(vpRect);
                        break;
                    }
                    case BinaryComponentType::SpriteRenderer:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& src = deserialized.AddComponent<SpriteRendererComponent>();
                        archive(src.Color.x, src.Color.y, src.Color.z, src.Color.w);
                        UUID texUuid;
                        archive(texUuid);
                        if (texUuid != UUID::EMPTY)
                            src.Texture = LoadAssetHandle<Texture>(texUuid);
                        break;
                    }
                    case BinaryComponentType::MeshRenderer:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& mc = deserialized.AddComponent<MeshRendererComponent>();
                        UUID meshUuid;
                        archive(meshUuid);
                        mc.MeshHandle = LoadAssetHandle<Mesh>(meshUuid);
                        mc.BaseMaterial = nullptr;
                        break;
                    }
                    case BinaryComponentType::Text:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        TextComponent& tc = deserialized.AddComponent<TextComponent>();
                        archive(tc.Text);
                        UUID fontUuid;
                        archive(fontUuid);
                        tc.Font = LoadAssetHandle<Font>(fontUuid);
                        archive(tc.Color.x, tc.Color.y, tc.Color.z, tc.Color.w);
                        archive(tc.Size);
                        archive(tc.AutoSize);
                        archive(tc.Wrapping);
                        archive(tc.OutlineColor.x, tc.OutlineColor.y, tc.OutlineColor.z, tc.OutlineColor.w);
                        archive(tc.Thickness);
                        archive(tc.CharacterSpacing);
                        archive(tc.WordSpacing);
                        archive(tc.LineSpacing);
                        archive(tc.UseKerning);
                        uint32_t fontStyle;
                        archive(fontStyle);
                        tc.FontStyle = (TextFontStyleBits)fontStyle;
                        uint32_t overflow, hAlign, vAlign;
                        archive(overflow);
                        archive(hAlign);
                        archive(vAlign);
                        tc.Overflow = (TextOverflow)overflow;
                        tc.HorizontalAlignment = (TextHorizontalAlignment)hAlign;
                        tc.VerticalAlignment = (TextVerticalAlignment)vAlign;
                        break;
                    }
                    case BinaryComponentType::AudioListener:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        deserialized.AddComponent<AudioListenerComponent>();
                        break;
                    }
                    case BinaryComponentType::AudioSource:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& asc = deserialized.AddComponent<AudioSourceComponent>();
                        UUID clipUuid;
                        archive(clipUuid);
                        float volume, pitch;
                        bool loop;
                        float minDist, maxDist;
                        bool playOnAwake, muted;
                        archive(volume);
                        archive(pitch);
                        archive(loop);
                        archive(minDist);
                        archive(maxDist);
                        archive(playOnAwake);
                        archive(muted);
                        asc.SetClip(LoadAssetHandle<AudioClip>(clipUuid));
                        asc.SetVolume(volume);
                        asc.SetPitch(pitch);
                        asc.SetLooping(loop);
                        asc.SetMinDistance(minDist);
                        asc.SetMaxDistance(maxDist);
                        asc.SetPlayOnAwake(playOnAwake);
                        asc.SetIsMuted(muted);
                        break;
                    }
                    case BinaryComponentType::MonoScript:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& msc = deserialized.AddComponent<MonoScriptComponent>();
                        uint32_t scriptCount;
                        archive(scriptCount);
                        for (uint32_t s = 0; s < scriptCount; s++)
                        {
                            String typeName;
                            archive(typeName);

                            SerializableObject obj;
                            Load(archive, obj);
                            Ref<SerializableObject> objRef = CreateRef<SerializableObject>(std::move(obj));

                            MonoClass* monoClass = MonoManager::Get().FindClass("Sandbox", typeName);
                            CW_ENGINE_ASSERT(monoClass != nullptr);
                            ::MonoClass* rawClass = monoClass->GetInternalPtr();
                            MonoReflectionType* runtimeType = MonoUtils::GetType(rawClass);

                            msc.Scripts.push_back(MonoScript(runtimeType));
                            msc.Scripts.back().m_SerializedObjectData = objRef;
                            msc.Scripts.back().Create(deserialized);
                        }
                        break;
                    }
                    case BinaryComponentType::Rigidbody2D:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& rb2dc = deserialized.AddComponent<Rigidbody2DComponent>();
                        uint32_t bodyType;
                        archive(bodyType);
                        rb2dc.SetBodyType((RigidbodyBodyType)bodyType);
                        float mass, gravityScale;
                        archive(mass);
                        archive(gravityScale);
                        rb2dc.SetMass(mass);
                        rb2dc.SetGravityScale(gravityScale);
                        uint32_t constraints, collisionDetectionMode, sleepMode;
                        archive(constraints);
                        archive(collisionDetectionMode);
                        archive(sleepMode);
                        rb2dc.SetConstraints((Rigidbody2DConstraints)constraints);
                        rb2dc.SetCollisionDetectionMode((CollisionDetectionMode2D)collisionDetectionMode);
                        rb2dc.SetSleepMode((RigidbodySleepMode)sleepMode);
                        float linearDrag, angularDrag;
                        archive(linearDrag);
                        archive(angularDrag);
                        rb2dc.SetLinearDrag(linearDrag);
                        rb2dc.SetAngularDrag(angularDrag);
                        uint32_t layerMask;
                        archive(layerMask);
                        rb2dc.SetLayerMask(layerMask, deserialized);
                        bool autoMass;
                        archive(autoMass);
                        rb2dc.SetAutoMass(autoMass, deserialized);
                        uint32_t interpolation;
                        archive(interpolation);
                        rb2dc.SetInterpolationMode((RigidbodyInterpolation)interpolation);
                        break;
                    }
                    case BinaryComponentType::BoxCollider2D:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& bc2dc = deserialized.AddComponent<BoxCollider2DComponent>();
                        glm::vec2 offset, size;
                        archive(offset.x, offset.y);
                        archive(size.x, size.y);
                        bool isTrigger;
                        archive(isTrigger);
                        UUID matUuid;
                        archive(matUuid);
                        bc2dc.SetOffset(offset, deserialized);
                        bc2dc.SetSize(size, deserialized);
                        bc2dc.SetIsTrigger(isTrigger);
                        if (matUuid != UUID::EMPTY)
                            bc2dc.SetMaterial(LoadAssetHandle<PhysicsMaterial2D>(matUuid));
                        else
                            bc2dc.SetMaterial(Physics2D::Get().GetDefaultMaterial());
                        break;
                    }
                    case BinaryComponentType::CircleCollider2D:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& cc2dc = deserialized.AddComponent<CircleCollider2DComponent>();
                        glm::vec2 offset;
                        archive(offset.x, offset.y);
                        float radius;
                        archive(radius);
                        bool isTrigger;
                        archive(isTrigger);
                        UUID matUuid;
                        archive(matUuid);
                        cc2dc.SetOffset(offset, deserialized);
                        cc2dc.SetRadius(radius, deserialized);
                        cc2dc.SetIsTrigger(isTrigger);
                        if (matUuid != UUID::EMPTY)
                            cc2dc.SetMaterial(LoadAssetHandle<PhysicsMaterial2D>(matUuid));
                        else
                            cc2dc.SetMaterial(Physics2D::Get().GetDefaultMaterial());
                        break;
                    }
                    case BinaryComponentType::Relationship:
                    {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        uint32_t childCount;
                        archive(childCount);
                        Vector<UUID> childUuids;
                        childUuids.reserve(childCount);
                        for (uint32_t ch = 0; ch < childCount; ch++)
                        {
                            UUID childUuid;
                            archive(childUuid);
                            childUuids.push_back(childUuid);
                        }
                        serializedRelationships[deserialized] = std::move(childUuids);
                        break;
                    }
                    default:
                        CW_ENGINE_ERROR("Unknown binary component type tag: {0}", typeTag);
                        break;
                    }
                }

                // If no component created the entity yet, create it now
                if (!deserialized)
                    deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
            }

            // Reconstruct relationships
            for (auto& [entity, childUuids] : serializedRelationships)
            {
                auto& rl = entity.GetComponent<RelationshipComponent>();
                for (const UUID& childUuid : childUuids)
                {
                    Entity e = m_Scene->GetEntityFromUuid(childUuid);
                    e.GetComponent<RelationshipComponent>().Parent = entity;
                    rl.Children.push_back(e);
                }
            }

            // Find root entity
            auto view = m_Scene->GetAllEntitiesWith<TagComponent>();
            Entity root;
            for (auto e : view)
            {
                Entity entity = { e, m_Scene.get() };
                if (!entity.GetParent())
                {
                    root = entity;
                    break;
                }
            }
            if (root)
                m_Scene->m_RootEntity = new Entity(root.GetHandle(), m_Scene.get());

            if (m_Scene->m_RootEntity)
                m_Scene->m_RootEntity->NotifyTransformChanged();

            // Deserialize time settings
            {
                Ref<TimeSettings> timeSettings = CreateRef<TimeSettings>();
                archive(timeSettings->TimeScale);
                archive(timeSettings->MaxTimestep);
                archive(timeSettings->FixedTimestep);
                if (Application::IsStartedUp())
                    Application::Get().SetTimeSettings(timeSettings);
            }

            // Deserialize physics settings
            {
                Ref<Physics2DSettings> physicsSettings = CreateRef<Physics2DSettings>();
                archive(physicsSettings->Gravity.x, physicsSettings->Gravity.y);
                archive(physicsSettings->VelocityIterations);
                archive(physicsSettings->PositionIterations);
                for (uint32_t i = 0; i < 32; i++)
                    archive(physicsSettings->LayerNames[i]);
                for (uint32_t i = 0; i < 32; i++)
                    archive(physicsSettings->MaskBits[i]);
                Physics2D::Get().SetPhysicsSettings(physicsSettings);
            }
        }
        catch (const std::exception& ex)
        {
            if (!m_Scene->m_RootEntity)
                m_Scene->CreateRootEntity();
            CW_ENGINE_ERROR("Error deserializing binary scene \"{0}\". {1}.", filepath, std::string(ex.what()));
        }

        stream->Close();
    }

} // namespace Crowny
