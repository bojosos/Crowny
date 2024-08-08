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

#include "Crowny/Serialization/SettingsSerializer.h"

#include "Crowny/Ecs/Components.h"

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

                    Ref<SerializableObject> serializableObject =
                      SerializableObject::CreateFromMonoObject(script.GetManagedInstance());
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
            SerializeValueYAML(out, "Thickess", tc.Thickess);
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

            SerializeValueYAML(out, "Color", sprite.Color); // TODO: Save textures

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
            if (bc2d.GetMaterial().GetUUID() !=
                Physics2D::Get().GetDefaultMaterial().GetUUID()) // TODO: fix this. shouldn't need to check anything
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
        out << YAML::Key << "Version" << YAML::Value << SceneVersion;
        out << YAML::EndMap;

        out << YAML::BeginMap;

        SerializeValueYAML(out, "Scene", m_Scene->GetName());

        SerializeValueYAML(out, "Entities", YAML::BeginSeq);
        m_Scene->m_Registry.sort<IDComponent>(
          [](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
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

    void SceneSerializer::SerializeBinary(const Path& filepath) {}

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
            if (version != SceneVersion) {
                CW_ENGINE_INFO("Loading scene with version: {0}, current: {1}", version, SceneVersion);
            }
            const YAML::Node sceneNode = data["Scene"];
            if (!sceneNode)
                return;
            UnorderedMap<Entity, YAML::Node> serializedComponents;
            const String sceneName = sceneNode.as<String>();
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
                        deserialized.SetPosition(transform["Position"].as<glm::vec3>(glm::vec3()));
                        deserialized.SetRotation(transform["Rotation"].as<glm::quat>(glm::quat()));
                        deserialized.SetScale(transform["Scale"].as<glm::vec3>(glm::vec3()));
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

                    const YAML::Node& text = entity["TextComponent"];
                    if (text)
                    {
                        TextComponent& tc = deserialized.AddComponent<TextComponent>();
                        tc.Text = text["Text"].as<String>();
                        tc.Font = LoadAssetHandle<Font>(text["Font"].as<UUID>(UUID::EMPTY));
                        tc.Color = text["Color"].as<glm::vec4>(glm::vec4(1.0f));
                        tc.Size = text["Size"].as<float>(0.0f);
                        tc.AutoSize = text["Size"].as<bool>(false);
                        tc.Wrapping = text["Wrapping"].as<bool>(false);
                        tc.FontStyle = (TextFontStyleBits)text["FontStyle"].as<uint32_t>(0);
                        tc.OutlineColor = text["OutlineColor"].as<glm::vec4>(glm::vec4(0.0f));
                        tc.Thickess = text["Thickess"].as<float>(0.8f);
                        tc.CharacterSpacing = text["CharacterSpacing"].as<float>(0.0f);
                        tc.WordSpacing = text["WordSpacing"].as<float>(0.0f);
                        tc.LineSpacing = text["LineSpacing"].as<float>(0.0f);
                        tc.UseKerning = text["UseKerning"].as<bool>(true);
                        DeserializeEnumYAML(text, "Overflow", tc.Overflow, TextOverflow::Overflow);
                        DeserializeEnumYAML(text, "HorizontalAlignment", tc.HorizontalAlignment,
                                            TextHorizontalAlignment::Left);
                        DeserializeEnumYAML(text, "VerticalAlignment", tc.VerticalAlignment,
                                            TextVerticalAlignment::Top);
                    }

                    const YAML::Node& mesh = entity["MeshRendererComponent"];
                    if (mesh)
                    {
                        MeshRendererComponent& mc = deserialized.AddComponent<MeshRendererComponent>();
                        mc.MeshHandle = LoadAssetHandle<Mesh>(mesh["Mesh"].as<UUID>(UUID::EMPTY));
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
                        rb2dc.SetCollisionDetectionMode(
                          (CollisionDetectionMode2D)rb2d["CollisionDetectionMode"].as<uint32_t>(0));
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

                            MonoClass* monoClass =
                              MonoManager::Get().FindClass("Sandbox", scriptNode.first.as<String>());
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

            const Ref<TimeSettings>& timeSettings = TimeSettingsSerializer::Deserialize(data);
            Application::Get().SetTimeSettings(timeSettings);
            const Ref<Physics2DSettings>& physicsSettings = Physics2DSettingsSerializer::Deserialize(data);
            Physics2D::Get().SetPhysicsSettings(physicsSettings);
        }
        catch (const std::exception& ex)
        {
            CW_ENGINE_ERROR("Error deserializing scene \"{0}\". {1}.", filepath, std::string(ex.what()));
        }
    }

    void SceneSerializer::DeserializeBinary(const Path& filepath) {}

} // namespace Crowny
