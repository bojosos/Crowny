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
#include "Crowny/NodeGraph/NodeGraphAsset.h"

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
            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
            for (const auto& mat : mesh.Materials)
                out << mat.GetUUID();
            out << YAML::EndSeq;
            EndYAMLMap(out, "MeshRendererComponent");
        }

        if (entity.HasComponent<ProceduralMeshComponent>())
        {
            const auto& pmc = entity.GetComponent<ProceduralMeshComponent>();
            BeginYAMLMap(out, "ProceduralMeshComponent");
            SerializeValueYAML(out, "Graph", pmc.Graph.GetUUID());
            out << YAML::Key << "InputValues" << YAML::Value << YAML::BeginSeq;
            for (const auto& [id, val] : pmc.InputValues)
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", id);
                PinDataType type = PinDataType::Float;
                if (pmc.Graph)
                {
                    if (const auto* input = pmc.Graph->GetGraph()->GetInput(id))
                        type = input->DataType;
                }
                SerializeEnumYAML(out, "Type", type);
                out << YAML::Key << "Value" << YAML::Value;
                switch (type)
                {
                case PinDataType::Float:
                    out << std::get<float>(val);
                    break;
                case PinDataType::Int:
                    out << std::get<int32_t>(val);
                    break;
                case PinDataType::Vec3:
                    out << std::get<glm::vec3>(val);
                    break;
                case PinDataType::Bool:
                    out << std::get<bool>(val);
                    break;
                default:
                    out << 0.0f;
                    break;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
            for (const auto& mat : pmc.Materials)
                out << mat.GetUUID();
            out << YAML::EndSeq;
            EndYAMLMap(out, "ProceduralMeshComponent");
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

        if (entity.HasComponent<PrefabComponent>())
        {
            const auto& pc = entity.GetComponent<PrefabComponent>();
            BeginYAMLMap(out, "PrefabComponent");
            SerializeValueYAML(out, "PrefabAsset", pc.PrefabAssetUuid);
            SerializeValueYAML(out, "PrefabEntity", pc.PrefabEntityUuid);
            out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginSeq;
            for (const auto& ovr : pc.Overrides)
                out << ovr;
            out << YAML::EndSeq;
            EndYAMLMap(out, "PrefabComponent");
        }

        EndYAMLMap(out, "Entity");
    }

    static void DeserializePinValueYAML(const YAML::Node& node, PinDataType type, PinValue& outVal)
    {
        if (!node)
            return;
        switch (type)
        {
        case PinDataType::Float:
            outVal = node.as<float>(0.0f);
            break;
        case PinDataType::Int:
            outVal = node.as<int32_t>(0);
            break;
        case PinDataType::Vec2:
            outVal = node.as<glm::vec2>(glm::vec2(0.0f));
            break;
        case PinDataType::Vec3:
            outVal = node.as<glm::vec3>(glm::vec3(0.0f));
            break;
        case PinDataType::Vec4:
            outVal = node.as<glm::vec4>(glm::vec4(0.0f));
            break;
        case PinDataType::Bool:
            outVal = node.as<bool>(false);
            break;
        default:
            break;
        }
    }

    void SceneSerializer::DeserializeEntities(const YAML::Node& entitiesNode)
    {
        UnorderedMap<Entity, YAML::Node> serializedComponents;

        for (const YAML::Node& entity : entitiesNode)
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
                auto& src = deserialized.AddComponent<SpriteRendererComponent>();
                src.Color = sprite["Color"].as<glm::vec4>();
                src.Texture = gAssetManager->LoadFromUUID<Texture>(sprite["Texture"].as<UUID>(UUID::EMPTY));
            }

            const YAML::Node& text = entity["TextComponent"];
            if (text)
            {
                TextComponent& tc = deserialized.AddComponent<TextComponent>();
                tc.Text = text["Text"].as<String>();
                tc.Font = gAssetManager->LoadFromUUID<Font>(text["Font"].as<UUID>(UUID::EMPTY));
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
                mc.MeshHandle = gAssetManager->LoadFromUUID<Mesh>(mesh["Mesh"].as<UUID>(UUID::EMPTY));
                const YAML::Node& materialsNode = mesh["Materials"];
                if (materialsNode && materialsNode.IsSequence())
                {
                    for (const auto& matNode : materialsNode)
                        mc.Materials.push_back(gAssetManager->LoadFromUUID<Material>(matNode.as<UUID>(UUID::EMPTY)));
                }
            }

            const YAML::Node& procMesh = entity["ProceduralMeshComponent"];
            if (procMesh)
            {
                ProceduralMeshComponent& pmc = deserialized.AddComponent<ProceduralMeshComponent>();
                UUID graphId = procMesh["Graph"].as<UUID>(UUID::EMPTY);
                if (graphId != UUID::EMPTY)
                    pmc.Graph = gAssetManager->LoadFromUUID<NodeGraphAsset>(graphId);

                const YAML::Node& inputValuesNode = procMesh["InputValues"];
                if (inputValuesNode && inputValuesNode.IsSequence())
                {
                    for (auto inputValNode : inputValuesNode)
                    {
                        UUID id = inputValNode["ID"].as<UUID>();
                        PinDataType type;
                        DeserializeEnumYAML(inputValNode, "Type", type, PinDataType::Float);
                        PinValue val = DefaultPinValue(type);
                        DeserializePinValueYAML(inputValNode["Value"], type, val);
                        pmc.InputValues[id] = val;
                    }
                }

                const YAML::Node& materialsNode = procMesh["Materials"];
                if (materialsNode && materialsNode.IsSequence())
                {
                    for (const auto& matNode : materialsNode)
                        pmc.Materials.push_back(gAssetManager->LoadFromUUID<Material>(matNode.as<UUID>(UUID::EMPTY)));
                }
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
                asc.SetClip(gAssetManager->LoadFromUUID<AudioClip>(source["AudioClip"].as<UUID>(UUID::EMPTY)));
            }

            auto loadPhysicsMaterial = [&](const YAML::Node& node) {
                return gAssetManager->LoadFromUUID<PhysicsMaterial2D>(node["Material"].as<UUID>(UUID::EMPTY));
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

            const YAML::Node& prefabComp = entity["PrefabComponent"];
            if (prefabComp)
            {
                auto& pc = deserialized.AddComponent<PrefabComponent>();
                pc.PrefabAssetUuid = prefabComp["PrefabAsset"].as<UUID>(UUID::EMPTY);
                pc.PrefabEntityUuid = prefabComp["PrefabEntity"].as<UUID>(UUID::EMPTY);
                const YAML::Node& overrides = prefabComp["Overrides"];
                if (overrides && overrides.IsSequence())
                {
                    for (const auto& ovr : overrides)
                        pc.Overrides.insert(ovr.as<String>());
                }
            }

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
                serializedComponents[deserialized] = rel["Children"];
        }

        for (auto& [entity, children] : serializedComponents)
        {
            RelationshipComponent& rl = const_cast<Entity&>(entity).GetComponent<RelationshipComponent>();
            for (const auto& child : children)
            {
                Entity e = m_Scene->GetEntityFromUuid(child.as<UUID>());
                if (e)
                {
                    e.GetComponent<RelationshipComponent>().Parent = entity;
                    rl.Children.push_back(e);
                }
            }
        }
    }

    void SceneSerializer::Serialize(const Path& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", SceneVersion);
        SerializeValueYAML(out, "Scene", m_Scene->GetName());
        SerializeValueYAML(out, "ImGuiLayout", m_Scene->GetImGuiLayout());
        SerializeValueYAML(out, "Entities", YAML::BeginSeq);
        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });
        m_Scene->m_Registry.each([&](auto entityID) {
            Entity entity = { entityID, m_Scene.get() };
            SerializeEntity(out, entity);
        });
        out << YAML::EndSeq;
        TimeSettingsSerializer::Serialize(gApplication->GetTimeSettings(), out);
        Physics2DSettingsSerializer::Serialize(gPhysics2D->GetPhysicsSettings(), out);
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
        Rigidbody,
        BoxCollider2D,
        CircleCollider2D,
        Relationship,
        PrefabComp,
        ProceduralMesh
    };

    void SceneSerializer::SerializeBinary(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        BinaryDataStreamOutputArchive archive(stream);

        uint32_t version = SceneVersion;
        archive(version);
        String sceneName = m_Scene->GetName();
        archive(sceneName);
        String layout = m_Scene->GetImGuiLayout();
        archive(layout);

        m_Scene->m_Registry.sort<IDComponent>([](const IDComponent& lhs, const IDComponent& rhs) { return lhs.Uuid < rhs.Uuid; });

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

            uint32_t componentCount = 0;
            if (entity.HasComponent<TagComponent>())
                componentCount++;
            if (entity.HasComponent<TransformComponent>())
                componentCount++;
            if (entity.HasComponent<CameraComponent>())
                componentCount++;
            if (entity.HasComponent<SpriteRendererComponent>())
                componentCount++;
            if (entity.HasComponent<MeshRendererComponent>())
                componentCount++;
            if (entity.HasComponent<TextComponent>())
                componentCount++;
            if (entity.HasComponent<AudioListenerComponent>())
                componentCount++;
            if (entity.HasComponent<AudioSourceComponent>())
                componentCount++;
            if (entity.HasComponent<MonoScriptComponent>())
            {
                if (entity.GetComponent<MonoScriptComponent>().Scripts.size() > 0)
                    componentCount++;
            }
            if (entity.HasComponent<Rigidbody2DComponent>())
                componentCount++;
            if (entity.HasComponent<BoxCollider2DComponent>())
                componentCount++;
            if (entity.HasComponent<CircleCollider2DComponent>())
                componentCount++;
            if (entity.HasComponent<RelationshipComponent>())
                componentCount++;
            if (entity.HasComponent<PrefabComponent>())
                componentCount++;
            if (entity.HasComponent<ProceduralMeshComponent>())
                componentCount++;
            archive(componentCount);

            if (entity.HasComponent<TagComponent>())
            {
                archive((uint32_t)BinaryComponentType::Tag);
                String tag = entity.GetName();
                archive(tag);
            }
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
            if (entity.HasComponent<SpriteRendererComponent>())
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                archive((uint32_t)BinaryComponentType::SpriteRenderer);
                archive(sprite.Color.x, sprite.Color.y, sprite.Color.z, sprite.Color.w);
                UUID texUuid = sprite.Texture.GetUUID();
                archive(texUuid);
            }
            if (entity.HasComponent<MeshRendererComponent>())
            {
                const auto& mesh = entity.GetComponent<MeshRendererComponent>();
                archive((uint32_t)BinaryComponentType::MeshRenderer);
                UUID meshUuid = mesh.MeshHandle.GetUUID();
                archive(meshUuid);
            }
            if (entity.HasComponent<TextComponent>())
            {
                const auto& tc = entity.GetComponent<TextComponent>();
                archive((uint32_t)BinaryComponentType::Text);
                archive(tc.Text);
                UUID fontUuid = tc.Font.GetUUID();
                archive(fontUuid);
                archive(tc.Color.x, tc.Color.y, tc.Color.z, tc.Color.w);
                archive(tc.Size, tc.AutoSize, tc.Wrapping);
                archive(tc.OutlineColor.x, tc.OutlineColor.y, tc.OutlineColor.z, tc.OutlineColor.w);
                archive(tc.Thickness, tc.CharacterSpacing, tc.WordSpacing, tc.LineSpacing, tc.UseKerning);
                archive((uint32_t)tc.FontStyle);
                archive((uint32_t)tc.Overflow);
                archive((uint32_t)tc.HorizontalAlignment);
                archive((uint32_t)tc.VerticalAlignment);
            }
            if (entity.HasComponent<AudioListenerComponent>())
            {
                archive((uint32_t)BinaryComponentType::AudioListener);
            }
            if (entity.HasComponent<AudioSourceComponent>())
            {
                const auto& asc = entity.GetComponent<AudioSourceComponent>();
                archive((uint32_t)BinaryComponentType::AudioSource);
                archive(asc.GetClip().GetUUID());
                archive(asc.GetVolume(), asc.GetPitch(), asc.GetLooping());
                archive(asc.GetMinDistance(), asc.GetMaxDistance(), asc.GetPlayOnAwake(), asc.GetIsMuted());
            }
            if (entity.HasComponent<MonoScriptComponent>())
            {
                const auto& msc = entity.GetComponent<MonoScriptComponent>();
                if (msc.Scripts.size() > 0)
                {
                    archive((uint32_t)BinaryComponentType::MonoScript);
                    archive((uint32_t)msc.Scripts.size());
                    for (const auto& script : msc.Scripts)
                    {
                        archive(script.GetTypeName());
                        Ref<SerializableObject> serializableObject = SerializableObject::CreateFromMonoObject(script.GetManagedInstance());
                        Save(archive, *serializableObject);
                    }
                }
            }
            if (entity.HasComponent<Rigidbody2DComponent>())
            {
                const auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
                archive((uint32_t)BinaryComponentType::Rigidbody);
                archive((uint32_t)rb2d.GetBodyType());
                archive(rb2d.GetMass(), rb2d.GetGravityScale(), (uint32_t)rb2d.GetConstraints());
                archive((uint32_t)rb2d.GetCollisionDetectionMode(), (uint32_t)rb2d.GetSleepMode());
                archive(rb2d.GetLinearDrag(), rb2d.GetAngularDrag(), rb2d.GetLayerMask());
                archive(rb2d.GetAutoMass(), (uint32_t)rb2d.GetInterpolationMode());
            }
            if (entity.HasComponent<BoxCollider2DComponent>())
            {
                const auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
                archive((uint32_t)BinaryComponentType::BoxCollider2D);
                archive(bc2d.GetOffset().x, bc2d.GetOffset().y);
                archive(bc2d.GetSize().x, bc2d.GetSize().y);
                archive(bc2d.IsTrigger());
                archive(bc2d.GetMaterial().GetUUID());
            }
            if (entity.HasComponent<CircleCollider2DComponent>())
            {
                const auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
                archive((uint32_t)BinaryComponentType::CircleCollider2D);
                archive(cc2d.GetOffset().x, cc2d.GetOffset().y);
                archive(cc2d.GetRadius());
                archive(cc2d.IsTrigger());
                archive(cc2d.GetMaterial().GetUUID());
            }
            if (entity.HasComponent<RelationshipComponent>())
            {
                const auto& rc = entity.GetComponent<RelationshipComponent>();
                archive((uint32_t)BinaryComponentType::Relationship);
                archive((uint32_t)rc.Children.size());
                for (Entity e : rc.Children)
                    archive(e.GetUuid());
            }
            if (entity.HasComponent<PrefabComponent>())
            {
                const auto& pc = entity.GetComponent<PrefabComponent>();
                archive((uint32_t)BinaryComponentType::PrefabComp);
                archive(pc.PrefabAssetUuid, pc.PrefabEntityUuid);
                archive((uint32_t)pc.Overrides.size());
                for (const auto& ovr : pc.Overrides)
                    archive(ovr);
            }
            if (entity.HasComponent<ProceduralMeshComponent>())
            {
                const auto& pmc = entity.GetComponent<ProceduralMeshComponent>();
                archive((uint32_t)BinaryComponentType::ProceduralMesh);
                archive(pmc.Graph.GetUUID());
                archive((uint32_t)pmc.InputValues.size());
                for (const auto& [id, val] : pmc.InputValues)
                {
                    archive(id);
                    PinDataType type = PinDataType::Float;
                    if (pmc.Graph)
                    {
                        if (const auto* input = pmc.Graph->GetGraph()->GetInput(id))
                            type = input->DataType;
                    }
                    archive((uint8_t)type);
                    switch (type)
                    {
                    case PinDataType::Float:
                        archive(std::get<float>(val));
                        break;
                    case PinDataType::Int:
                        archive(std::get<int32_t>(val));
                        break;
                    case PinDataType::Vec3: {
                        glm::vec3 v = std::get<glm::vec3>(val);
                        archive(v.x, v.y, v.z);
                        break;
                    }
                    case PinDataType::Bool:
                        archive(std::get<bool>(val));
                        break;
                    default:
                        archive(0.0f);
                        break;
                    }
                }
                archive((uint32_t)pmc.Materials.size());
                for (const auto& mat : pmc.Materials)
                    archive(mat.GetUUID());
            }
        });

        {
            const auto& ts = gApplication->GetTimeSettings();
            archive(ts->TimeScale, ts->MaxTimestep, ts->FixedTimestep);
        }
        {
            const auto& ps = gPhysics2D->GetPhysicsSettings();
            archive(ps->Gravity.x, ps->Gravity.y);
            archive(ps->VelocityIterations, ps->PositionIterations);
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
            if (!data["Scene"])
                return;
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
            if (Application::IsStartedUp())
                gApplication->SetTimeSettings(TimeSettingsSerializer::Deserialize(data));
            gPhysics2D->SetPhysicsSettings(Physics2DSettingsSerializer::Deserialize(data));
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
            String sceneName;
            archive(sceneName);
            String layout;
            archive(layout);
            m_Scene->m_Registry.clear();
            m_Scene->m_EntityMap.clear();
            delete m_Scene->m_RootEntity;
            m_Scene->m_RootEntity = nullptr;
            m_Scene->m_Name = sceneName;
            m_Scene->m_ImGuiLayout = layout;
            m_Scene->m_Filepath = filepath;
            uint32_t entityCount;
            archive(entityCount);
            UnorderedMap<Entity, Vector<UUID>> serializedRelationships;
            for (uint32_t i = 0; i < entityCount; i++)
            {
                UUID uuid;
                archive(uuid);
                String name;
                archive(name);
                uint32_t componentCount;
                archive(componentCount);
                Entity deserialized;
                for (uint32_t c = 0; c < componentCount; c++)
                {
                    uint32_t typeTag;
                    archive(typeTag);
                    BinaryComponentType componentType = (BinaryComponentType)typeTag;
                    switch (componentType)
                    {
                    case BinaryComponentType::Tag: {
                        String tag;
                        archive(tag);
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, tag);
                        break;
                    }
                    case BinaryComponentType::Transform: {
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
                    case BinaryComponentType::Camera: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& cc = deserialized.AddComponent<CameraComponent>();
                        uint32_t projType;
                        archive(projType);
                        cc.Camera.SetProjectionType((SceneCamera::CameraProjection)projType);
                        float fov, nearP, farP;
                        archive(fov, nearP, farP);
                        cc.Camera.SetPerspectiveVerticalFOV(fov);
                        cc.Camera.SetPerspectiveNearClip(nearP);
                        cc.Camera.SetPerspectiveFarClip(farP);
                        float size, onear, ofar;
                        archive(size, onear, ofar);
                        cc.Camera.SetOrthographicSize(size);
                        cc.Camera.SetOrthographicNearClip(onear);
                        cc.Camera.SetOrthographicFarClip(ofar);
                        bool hdr, msaa, occ;
                        archive(hdr, msaa, occ);
                        cc.Camera.SetHDR(hdr);
                        cc.Camera.SetMSAA(msaa);
                        cc.Camera.SetOcclusionCulling(occ);
                        glm::vec3 bg;
                        archive(bg.x, bg.y, bg.z);
                        cc.Camera.SetBackgroundColor(bg);
                        glm::vec4 vp;
                        archive(vp.x, vp.y, vp.z, vp.w);
                        cc.Camera.SetViewportRect(vp);
                        break;
                    }
                    case BinaryComponentType::SpriteRenderer: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& src = deserialized.AddComponent<SpriteRendererComponent>();
                        archive(src.Color.x, src.Color.y, src.Color.z, src.Color.w);
                        UUID tex;
                        archive(tex);
                        if (tex != UUID::EMPTY)
                            src.Texture = gAssetManager->LoadFromUUID<Texture>(tex);
                        break;
                    }
                    case BinaryComponentType::MeshRenderer: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& mc = deserialized.AddComponent<MeshRendererComponent>();
                        UUID mesh;
                        archive(mesh);
                        mc.MeshHandle = gAssetManager->LoadFromUUID<Mesh>(mesh);
                        break;
                    }
                    case BinaryComponentType::Text: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& tc = deserialized.AddComponent<TextComponent>();
                        archive(tc.Text);
                        UUID f;
                        archive(f);
                        tc.Font = gAssetManager->LoadFromUUID<Font>(f);
                        archive(tc.Color.x, tc.Color.y, tc.Color.z, tc.Color.w);
                        archive(tc.Size, tc.AutoSize, tc.Wrapping);
                        archive(tc.OutlineColor.x, tc.OutlineColor.y, tc.OutlineColor.z, tc.OutlineColor.w);
                        archive(tc.Thickness, tc.CharacterSpacing, tc.WordSpacing, tc.LineSpacing, tc.UseKerning);
                        uint32_t style, over, ha, va;
                        archive(style, over, ha, va);
                        tc.FontStyle = (TextFontStyleBits)style;
                        tc.Overflow = (TextOverflow)over;
                        tc.HorizontalAlignment = (TextHorizontalAlignment)ha;
                        tc.VerticalAlignment = (TextVerticalAlignment)va;
                        break;
                    }
                    case BinaryComponentType::AudioListener: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        deserialized.AddComponent<AudioListenerComponent>();
                        break;
                    }
                    case BinaryComponentType::AudioSource: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& asc = deserialized.AddComponent<AudioSourceComponent>();
                        UUID c;
                        archive(c);
                        float v, p, minD, maxD;
                        bool l, pa, m;
                        archive(v, p, l, minD, maxD, pa, m);
                        asc.SetClip(gAssetManager->LoadFromUUID<AudioClip>(c));
                        asc.SetVolume(v);
                        asc.SetPitch(p);
                        asc.SetLooping(l);
                        asc.SetMinDistance(minD);
                        asc.SetMaxDistance(maxD);
                        asc.SetPlayOnAwake(pa);
                        asc.SetIsMuted(m);
                        break;
                    }
                    case BinaryComponentType::MonoScript: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& msc = deserialized.AddComponent<MonoScriptComponent>();
                        uint32_t sc;
                        archive(sc);
                        for (uint32_t s = 0; s < sc; s++)
                        {
                            String tn;
                            archive(tn);
                            SerializableObject obj;
                            Load(archive, obj);
                            Ref<SerializableObject> objRef = CreateRef<SerializableObject>(std::move(obj));
                            MonoClass* mc = MonoManager::Get().FindClass("Sandbox", tn);
                            CW_ENGINE_ASSERT(mc);
                            msc.Scripts.push_back(MonoScript(MonoUtils::GetType(mc->GetInternalPtr())));
                            msc.Scripts.back().m_SerializedObjectData = objRef;
                            msc.Scripts.back().Create(deserialized);
                        }
                        break;
                    }
                    case BinaryComponentType::Rigidbody: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& rb = deserialized.AddComponent<Rigidbody2DComponent>();
                        uint32_t bt, con, cdm, sm, inter;
                        float m, gs, ld, ad;
                        uint32_t lm;
                        bool am;
                        archive(bt, m, gs, con, cdm, sm, ld, ad, lm, am, inter);
                        rb.SetBodyType((RigidbodyBodyType)bt);
                        rb.SetMass(m);
                        rb.SetGravityScale(gs);
                        rb.SetConstraints((Rigidbody2DConstraints)con);
                        rb.SetCollisionDetectionMode((CollisionDetectionMode2D)cdm);
                        rb.SetSleepMode((RigidbodySleepMode)sm);
                        rb.SetLinearDrag(ld);
                        rb.SetAngularDrag(ad);
                        rb.SetLayerMask(lm, deserialized);
                        rb.SetAutoMass(am, deserialized);
                        rb.SetInterpolationMode((RigidbodyInterpolation)inter);
                        break;
                    }
                    case BinaryComponentType::BoxCollider2D: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& bc = deserialized.AddComponent<BoxCollider2DComponent>();
                        glm::vec2 o, s;
                        bool it;
                        UUID mat;
                        archive(o.x, o.y, s.x, s.y, it, mat);
                        bc.SetOffset(o, deserialized);
                        bc.SetSize(s, deserialized);
                        bc.SetIsTrigger(it);
                        bc.SetMaterial(gAssetManager->LoadFromUUID<PhysicsMaterial2D>(mat));
                        break;
                    }
                    case BinaryComponentType::CircleCollider2D: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& cc = deserialized.AddComponent<CircleCollider2DComponent>();
                        glm::vec2 o;
                        float r;
                        bool it;
                        UUID mat;
                        archive(o.x, o.y, r, it, mat);
                        cc.SetOffset(o, deserialized);
                        cc.SetRadius(r, deserialized);
                        cc.SetIsTrigger(it);
                        cc.SetMaterial(gAssetManager->LoadFromUUID<PhysicsMaterial2D>(mat));
                        break;
                    }
                    case BinaryComponentType::Relationship: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        uint32_t rcc;
                        archive(rcc);
                        Vector<UUID> cu;
                        for (uint32_t ch = 0; ch < rcc; ch++)
                        {
                            UUID c;
                            archive(c);
                            cu.push_back(c);
                        }
                        serializedRelationships[deserialized] = std::move(cu);
                        break;
                    }
                    case BinaryComponentType::PrefabComp: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& pc = deserialized.AddComponent<PrefabComponent>();
                        archive(pc.PrefabAssetUuid, pc.PrefabEntityUuid);
                        uint32_t oc;
                        archive(oc);
                        for (uint32_t i = 0; i < oc; i++)
                        {
                            String o;
                            archive(o);
                            pc.Overrides.insert(o);
                        }
                        break;
                    }
                    case BinaryComponentType::ProceduralMesh: {
                        if (!deserialized)
                            deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
                        auto& pmc = deserialized.AddComponent<ProceduralMeshComponent>();
                        UUID gu;
                        archive(gu);
                        if (gu != UUID::EMPTY)
                            pmc.Graph = gAssetManager->LoadFromUUID<NodeGraphAsset>(gu);
                        uint32_t ic;
                        archive(ic);
                        for (uint32_t j = 0; j < ic; j++)
                        {
                            UUID id;
                            archive(id);
                            uint8_t tr;
                            archive(tr);
                            PinDataType t = (PinDataType)tr;
                            PinValue v = DefaultPinValue(t);
                            switch (t)
                            {
                            case PinDataType::Float: {
                                float val;
                                archive(val);
                                v = val;
                                break;
                            }
                            case PinDataType::Int: {
                                int32_t val;
                                archive(val);
                                v = val;
                                break;
                            }
                            case PinDataType::Vec3: {
                                glm::vec3 val;
                                archive(val.x, val.y, val.z);
                                v = val;
                                break;
                            }
                            case PinDataType::Bool: {
                                bool val;
                                archive(val);
                                v = val;
                                break;
                            }
                            }
                            pmc.InputValues[id] = v;
                        }
                        uint32_t pmcc;
                        archive(pmcc);
                        for (uint32_t j = 0; j < pmcc; j++)
                        {
                            UUID mu;
                            archive(mu);
                            pmc.Materials.push_back(gAssetManager->LoadFromUUID<Material>(mu));
                        }
                        break;
                    }
                    default:
                        CW_ENGINE_ERROR("Unknown binary component type tag: {0}", typeTag);
                        break;
                    }
                }
                if (!deserialized)
                    deserialized = m_Scene->CreateEntityWithUuid(uuid, name);
            }
            for (auto& [e, cu] : serializedRelationships)
            {
                auto& rl = const_cast<Entity&>(e).GetComponent<RelationshipComponent>();
                for (const auto& c : cu)
                {
                    Entity ch = m_Scene->GetEntityFromUuid(c);
                    if (ch)
                    {
                        ch.GetComponent<RelationshipComponent>().Parent = e;
                        rl.Children.push_back(ch);
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
            {
                Ref<TimeSettings> ts = CreateRef<TimeSettings>();
                archive(ts->TimeScale, ts->MaxTimestep, ts->FixedTimestep);
                if (Application::IsStartedUp())
                    gApplication->SetTimeSettings(ts);
            }
            {
                Ref<Physics2DSettings> ps = CreateRef<Physics2DSettings>();
                archive(ps->Gravity.x, ps->Gravity.y, ps->VelocityIterations, ps->PositionIterations);
                for (uint32_t i = 0; i < 32; i++)
                    archive(ps->LayerNames[i]);
                for (uint32_t i = 0; i < 32; i++)
                    archive(ps->MaskBits[i]);
                gPhysics2D->SetPhysicsSettings(ps);
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
