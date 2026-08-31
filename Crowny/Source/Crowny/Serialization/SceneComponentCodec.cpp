#include "cwpch.h"

#include "Crowny/Serialization/SceneComponentCodec.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include <cereal/types/string.hpp>

#include <array>
#include <stdexcept>

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
            return AssetManager::TryGet() != nullptr && AssetManager::TryGet()->IsAssetRegistered(material.GetUUID());
        }

        constexpr uint32_t PhysicsMaterialOverrideVersion = 1;

        void WritePhysicsMaterialValuesYaml(YAML::Emitter& out, const PhysicsMaterialData& data)
        {
            SerializeValueYAML(out, "Density", data.Density);
            SerializeValueYAML(out, "Friction", data.Friction);
            SerializeValueYAML(out, "Restitution", data.Restitution);
            SerializeValueYAML(out, "RestitutionThreshold", data.RestitutionThreshold);
            SerializeEnumYAML(out, "FrictionCombine", data.FrictionCombine);
            SerializeEnumYAML(out, "RestitutionCombine", data.RestitutionCombine);
        }

        PhysicsMaterialData ReadPhysicsMaterialValuesYaml(const YAML::Node& node)
        {
            PhysicsMaterialData data;
            data.Density = node["Density"].as<float>(data.Density);
            data.Friction = node["Friction"].as<float>(data.Friction);
            data.Restitution = node["Restitution"].as<float>(data.Restitution);
            data.RestitutionThreshold = node["RestitutionThreshold"].as<float>(data.RestitutionThreshold);
            DeserializeEnumYAML(node, "FrictionCombine", data.FrictionCombine, data.FrictionCombine);
            DeserializeEnumYAML(node, "RestitutionCombine", data.RestitutionCombine, data.RestitutionCombine);
            return NormalizePhysicsMaterialData(data);
        }

        void WritePhysicsMaterialOverrideYaml(YAML::Emitter& out, const PhysicsMaterialOverride& materialOverride)
        {
            const PhysicsMaterialOverride normalized = NormalizePhysicsMaterialOverride(materialOverride);
            if (!normalized.Fields)
                return;
            BeginYAMLMap(out, "MaterialOverride");
            SerializeValueYAML(out, "Version", PhysicsMaterialOverrideVersion);
            SerializeValueYAML(out, "Fields", static_cast<uint32_t>(normalized.Fields));
            WritePhysicsMaterialValuesYaml(out, normalized.Values);
            EndYAMLMap(out, "MaterialOverride");
        }

        PhysicsMaterialOverride ReadPhysicsMaterialOverrideYaml(const YAML::Node& node)
        {
            const YAML::Node materialOverride = node["MaterialOverride"];
            if (!materialOverride)
                return {};
            const uint32_t version = materialOverride["Version"].as<uint32_t>(0);
            if (version != PhysicsMaterialOverrideVersion)
                throw std::runtime_error("Unsupported physics material override version.");
            PhysicsMaterialOverride result;
            result.Fields = PhysicsMaterialOverrideFlags(materialOverride["Fields"].as<uint32_t>(0));
            result.Values = ReadPhysicsMaterialValuesYaml(materialOverride);
            return NormalizePhysicsMaterialOverride(result);
        }

        template <typename T>
        PhysicsMaterialOverride MakeSerializedPhysicsMaterialOverride(const AssetHandle<T>& material,
                                                                      const PhysicsMaterialOverride& configuredOverride,
                                                                      const PhysicsMaterialData& resolvedMaterial)
        {
            if (ShouldSerializeMaterialReference(material))
                return NormalizePhysicsMaterialOverride(configuredOverride);
            return MakePhysicsMaterialOverride(resolvedMaterial);
        }

        template <typename T>
        void WritePhysicsMaterialYaml(YAML::Emitter& out, const AssetHandle<T>& material,
                                      const PhysicsMaterialOverride& configuredOverride, const PhysicsMaterialData& resolvedMaterial)
        {
            if (ShouldSerializeMaterialReference(material))
                SerializeValueYAML(out, "Material", material.GetUUID());
            WritePhysicsMaterialOverrideYaml(out,
                                             MakeSerializedPhysicsMaterialOverride(material, configuredOverride, resolvedMaterial));
        }

        template <typename T>
        void WritePhysicsMaterialBinary(BinaryDataStreamOutputArchive& archive, const AssetHandle<T>& material,
                                        const PhysicsMaterialOverride& configuredOverride, const PhysicsMaterialData& resolvedMaterial)
        {
            const UUID materialId = ShouldSerializeMaterialReference(material) ? material.GetUUID() : UUID::EMPTY;
            const PhysicsMaterialOverride materialOverride =
              MakeSerializedPhysicsMaterialOverride(material, configuredOverride, resolvedMaterial);
            const PhysicsMaterialData& values = materialOverride.Values;
            archive(materialId, static_cast<uint32_t>(materialOverride.Fields));
            archive(values.Density, values.Friction, values.Restitution, values.RestitutionThreshold,
                    static_cast<uint8_t>(values.FrictionCombine), static_cast<uint8_t>(values.RestitutionCombine));
        }

        template <typename T> struct PhysicsMaterialBinding
        {
            AssetHandle<T> Material;
            PhysicsMaterialOverride Override;
        };

        template <typename T> PhysicsMaterialBinding<T> ReadPhysicsMaterialBinary(BinaryDataStreamInputArchive& archive)
        {
            UUID materialId;
            uint32_t fields = 0;
            PhysicsMaterialData values;
            uint8_t frictionCombine = 0;
            uint8_t restitutionCombine = 0;
            archive(materialId, fields);
            archive(values.Density, values.Friction, values.Restitution, values.RestitutionThreshold, frictionCombine, restitutionCombine);
            values.FrictionCombine = static_cast<PhysicsCombineMode>(frictionCombine);
            values.RestitutionCombine = static_cast<PhysicsCombineMode>(restitutionCombine);
            return { LoadAssetReference<T>(materialId), NormalizePhysicsMaterialOverride({ PhysicsMaterialOverrideFlags(fields), values }) };
        }

        void ReadPinValueYaml(const YAML::Node& node, PinDataType type, PinValue& outValue)
        {
            if (!node)
                return;
            switch (type)
            {
            case PinDataType::Float:
                outValue = node.as<float>(0.0f);
                break;
            case PinDataType::Int:
                outValue = node.as<int32_t>(0);
                break;
            case PinDataType::Vec2:
                outValue = node.as<glm::vec2>(glm::vec2(0.0f));
                break;
            case PinDataType::Vec3:
                outValue = node.as<glm::vec3>(glm::vec3(0.0f));
                break;
            case PinDataType::Vec4:
                outValue = node.as<glm::vec4>(glm::vec4(0.0f));
                break;
            case PinDataType::Bool:
                outValue = node.as<bool>(false);
                break;
            default:
                break;
            }
        }

        template <typename T> bool HasComponent(Entity entity) { return entity.HasComponent<T>(); }
        template <typename T> bool AlwaysSerialize(Entity) { return true; }

        bool ShouldSerializeManagedScript(Entity entity) { return !entity.GetComponent<ManagedScriptComponent>().Scripts.empty(); }

        template <typename T> struct ComponentIO;

        template <> struct ComponentIO<TagComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity) { SerializeValueYAML(out, "Tag", entity.GetName()); }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                entity.GetComponent<TagComponent>().Tag = node["Tag"].as<String>(entity.GetName());
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                String tag = entity.GetName();
                archive(tag);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                String tag;
                archive(tag);
                entity.GetComponent<TagComponent>().Tag = std::move(tag);
            }
        };

        template <> struct ComponentIO<TransformComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                SerializeValueYAML(out, "Position", entity.GetLocalPosition());
                SerializeValueYAML(out, "Rotation", entity.GetLocalRotation());
                SerializeValueYAML(out, "Scale", entity.GetLocalScale());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& transform = entity.GetTransform();
                transform.SetPosition(node["Position"].as<glm::vec3>(glm::vec3()));
                transform.SetRotation(node["Rotation"].as<glm::quat>(glm::quat()));
                transform.SetScale(node["Scale"].as<glm::vec3>(glm::vec3()));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const glm::vec3 position = entity.GetLocalPosition();
                const glm::quat rotation = entity.GetLocalRotation();
                const glm::vec3 scale = entity.GetLocalScale();
                archive(position.x, position.y, position.z);
                archive(rotation.x, rotation.y, rotation.z, rotation.w);
                archive(scale.x, scale.y, scale.z);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                glm::vec3 position;
                glm::quat rotation;
                glm::vec3 scale;
                archive(position.x, position.y, position.z);
                archive(rotation.x, rotation.y, rotation.z, rotation.w);
                archive(scale.x, scale.y, scale.z);
                auto& transform = entity.GetTransform();
                transform.SetPosition(position);
                transform.SetRotation(rotation);
                transform.SetScale(scale);
            }
        };

        template <> struct ComponentIO<CameraComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& camera = entity.GetComponent<CameraComponent>().Camera;
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
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& camera = entity.AddComponent<CameraComponent>().Camera;
                if (const YAML::Node value = node["ProjectionType"])
                    camera.SetProjectionType(static_cast<SceneCamera::CameraProjection>(value.as<uint32_t>()));
                if (const YAML::Node value = node["PerspectiveFOV"])
                    camera.SetPerspectiveVerticalFOV(value.as<float>());
                if (const YAML::Node value = node["PerspectiveNear"])
                    camera.SetPerspectiveNearClip(value.as<float>());
                if (const YAML::Node value = node["PerspectiveFar"])
                    camera.SetPerspectiveFarClip(value.as<float>());
                if (const YAML::Node value = node["OrthographicSize"])
                    camera.SetOrthographicSize(value.as<float>());
                if (const YAML::Node value = node["OrthographicNear"])
                    camera.SetOrthographicNearClip(value.as<float>());
                if (const YAML::Node value = node["OrthographicFar"])
                    camera.SetOrthographicFarClip(value.as<float>());
                if (const YAML::Node value = node["HDR"])
                    camera.SetHDR(value.as<bool>());
                if (const YAML::Node value = node["MSAA"])
                    camera.SetMSAA(value.as<bool>());
                if (const YAML::Node value = node["OcclusionCulling"])
                    camera.SetOcclusionCulling(value.as<bool>());
                if (const YAML::Node value = node["BackgroundColor"])
                    camera.SetBackgroundColor(value.as<glm::vec3>());
                if (const YAML::Node value = node["ViewportRect"])
                    camera.SetViewportRect(value.as<glm::vec4>());
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& camera = entity.GetComponent<CameraComponent>().Camera;
                archive(static_cast<uint32_t>(camera.GetProjectionType()));
                archive(camera.GetPerspectiveVerticalFOV(), camera.GetPerspectiveNearClip(), camera.GetPerspectiveFarClip());
                archive(camera.GetOrthographicSize(), camera.GetOrthographicNearClip(), camera.GetOrthographicFarClip());
                archive(camera.GetHDR(), camera.GetMSAA(), camera.GetOcclusionCulling());
                const glm::vec3 background = camera.GetBackgroundColor();
                archive(background.x, background.y, background.z);
                const glm::vec4 viewport = camera.GetViewportRect();
                archive(viewport.x, viewport.y, viewport.z, viewport.w);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& camera = entity.AddComponent<CameraComponent>().Camera;
                uint32_t projectionType;
                float fov, perspectiveNear, perspectiveFar;
                float size, orthographicNear, orthographicFar;
                bool hdr, msaa, occlusionCulling;
                glm::vec3 background;
                glm::vec4 viewport;
                archive(projectionType);
                archive(fov, perspectiveNear, perspectiveFar);
                archive(size, orthographicNear, orthographicFar);
                archive(hdr, msaa, occlusionCulling);
                archive(background.x, background.y, background.z);
                archive(viewport.x, viewport.y, viewport.z, viewport.w);
                camera.SetProjectionType(static_cast<SceneCamera::CameraProjection>(projectionType));
                camera.SetPerspectiveVerticalFOV(fov);
                camera.SetPerspectiveNearClip(perspectiveNear);
                camera.SetPerspectiveFarClip(perspectiveFar);
                camera.SetOrthographicSize(size);
                camera.SetOrthographicNearClip(orthographicNear);
                camera.SetOrthographicFarClip(orthographicFar);
                camera.SetHDR(hdr);
                camera.SetMSAA(msaa);
                camera.SetOcclusionCulling(occlusionCulling);
                camera.SetBackgroundColor(background);
                camera.SetViewportRect(viewport);
            }
        };

        template <> struct ComponentIO<SpriteRendererComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                SerializeValueYAML(out, "Color", sprite.Color);
                SerializeValueYAML(out, "Texture", sprite.Texture.GetUUID());
                SerializeValueYAML(out, "SortingLayer", sprite.SortingLayer);
                SerializeValueYAML(out, "OrderInLayer", sprite.OrderInLayer);
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& sprite = entity.AddComponent<SpriteRendererComponent>();
                sprite.Color = node["Color"].as<glm::vec4>();
                sprite.Texture = LoadAssetReference<Texture>(node["Texture"].as<UUID>(UUID::EMPTY));
                sprite.SortingLayer = node["SortingLayer"].as<int32_t>(0);
                sprite.OrderInLayer = node["OrderInLayer"].as<int32_t>(0);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                archive(sprite.Color.x, sprite.Color.y, sprite.Color.z, sprite.Color.w);
                archive(sprite.Texture.GetUUID(), sprite.SortingLayer, sprite.OrderInLayer);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& sprite = entity.AddComponent<SpriteRendererComponent>();
                archive(sprite.Color.x, sprite.Color.y, sprite.Color.z, sprite.Color.w);
                UUID texture;
                archive(texture);
                sprite.Texture = LoadAssetReference<Texture>(texture);
                archive(sprite.SortingLayer, sprite.OrderInLayer);
            }
        };

        template <> struct ComponentIO<MeshRendererComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& mesh = entity.GetComponent<MeshRendererComponent>();
                SerializeValueYAML(out, "Mesh", mesh.MeshHandle.GetUUID());
                SerializeValueYAML(out, "VisibilityLayers", mesh.VisibilityLayers.Value);
                SerializeValueYAML(out, "LodBias", mesh.LodBias);
                SerializeValueYAML(out, "RenderLayerOrder", mesh.RenderLayerOrder);
                SerializeValueYAML(out, "Visible", mesh.Visible);
                SerializeValueYAML(out, "CastShadows", mesh.CastShadows);
                SerializeValueYAML(out, "ReceiveShadows", mesh.ReceiveShadows);
                SerializeValueYAML(out, "MotionVectors", mesh.MotionVectors);
                out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
                for (const auto& material : mesh.Materials)
                    out << material.GetUUID();
                out << YAML::EndSeq;
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& mesh = entity.AddComponent<MeshRendererComponent>();
                mesh.MeshHandle = LoadAssetReference<Mesh>(node["Mesh"].as<UUID>(UUID::EMPTY));
                mesh.VisibilityLayers.Value = node["VisibilityLayers"].as<uint32_t>(0xffffffffu);
                mesh.LodBias = node["LodBias"].as<float>(0.0f);
                mesh.RenderLayerOrder = node["RenderLayerOrder"].as<int32_t>(0);
                mesh.Visible = node["Visible"].as<bool>(true);
                mesh.CastShadows = node["CastShadows"].as<bool>(true);
                mesh.ReceiveShadows = node["ReceiveShadows"].as<bool>(true);
                mesh.MotionVectors = node["MotionVectors"].as<bool>(true);
                const YAML::Node& materials = node["Materials"];
                if (materials && materials.IsSequence())
                {
                    for (const auto& material : materials)
                        mesh.Materials.push_back(LoadAssetReference<Material>(material.as<UUID>(UUID::EMPTY)));
                }
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& mesh = entity.GetComponent<MeshRendererComponent>();
                archive(mesh.MeshHandle.GetUUID());
                archive(mesh.VisibilityLayers.Value, mesh.LodBias, mesh.RenderLayerOrder, mesh.Visible, mesh.CastShadows, mesh.ReceiveShadows,
                        mesh.MotionVectors);
                archive(static_cast<uint32_t>(mesh.Materials.size()));
                for (const auto& material : mesh.Materials)
                    archive(material.GetUUID());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& mesh = entity.AddComponent<MeshRendererComponent>();
                UUID meshUuid;
                archive(meshUuid);
                mesh.MeshHandle = LoadAssetReference<Mesh>(meshUuid);
                archive(mesh.VisibilityLayers.Value, mesh.LodBias, mesh.RenderLayerOrder, mesh.Visible, mesh.CastShadows, mesh.ReceiveShadows,
                        mesh.MotionVectors);
                uint32_t materialCount = 0;
                archive(materialCount);
                mesh.Materials.reserve(materialCount);
                for (uint32_t index = 0; index < materialCount; index++)
                {
                    UUID material;
                    archive(material);
                    mesh.Materials.push_back(LoadAssetReference<Material>(material));
                }
            }
        };

        template <> struct ComponentIO<TextComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& text = entity.GetComponent<TextComponent>();
                SerializeValueYAML(out, "Text", text.Text);
                SerializeValueYAML(out, "Font", text.Font.GetUUID());
                SerializeValueYAML(out, "Color", text.Color);
                SerializeValueYAML(out, "Size", text.Size);
                SerializeValueYAML(out, "AutoSize", text.AutoSize);
                SerializeValueYAML(out, "AutoSizeMin", text.AutoSizeMin);
                SerializeValueYAML(out, "AutoSizeMax", text.AutoSizeMax);
                SerializeValueYAML(out, "LayoutSize", text.LayoutSize);
                SerializeValueYAML(out, "Wrapping", text.Wrapping);
                SerializeEnumYAML(out, "WrapMode", text.WrapMode);
                SerializeValueYAML(out, "ClipToBounds", text.ClipToBounds);
                SerializeValueYAML(out, "MaxLines", text.MaxLines);
                SerializeValueYAML(out, "OutlineColor", text.OutlineColor);
                SerializeValueYAML(out, "Thickness", text.Thickness);
                SerializeValueYAML(out, "ShadowColor", text.ShadowColor);
                SerializeValueYAML(out, "ShadowOffset", text.ShadowOffset);
                SerializeValueYAML(out, "ShadowSoftness", text.ShadowSoftness);
                SerializeValueYAML(out, "CharacterSpacing", text.CharacterSpacing);
                SerializeValueYAML(out, "WordSpacing", text.WordSpacing);
                SerializeValueYAML(out, "LineSpacing", text.LineSpacing);
                SerializeValueYAML(out, "ParagraphSpacing", text.ParagraphSpacing);
                SerializeValueYAML(out, "TabWidth", text.TabWidth);
                SerializeValueYAML(out, "UseCustomDecorationColor", text.UseCustomDecorationColor);
                SerializeValueYAML(out, "DecorationColor", text.DecorationColor);
                SerializeValueYAML(out, "DecorationThickness", text.DecorationThickness);
                SerializeValueYAML(out, "UnderlineOffset", text.UnderlineOffset);
                SerializeValueYAML(out, "StrikethroughOffset", text.StrikethroughOffset);
                SerializeValueYAML(out, "UseKerning", text.UseKerning);
                SerializeValueYAML(out, "FontStyle", static_cast<uint32_t>(text.FontStyle));
                SerializeEnumYAML(out, "Overflow", text.Overflow);
                SerializeEnumYAML(out, "HorizontalAlignment", text.HorizontalAlignment);
                SerializeEnumYAML(out, "VerticalAlignment", text.VerticalAlignment);
                SerializeValueYAML(out, "SortingLayer", text.SortingLayer);
                SerializeValueYAML(out, "OrderInLayer", text.OrderInLayer);
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& text = entity.AddComponent<TextComponent>();
                text.Text = node["Text"].as<String>();
                text.Font = LoadAssetReference<Font>(node["Font"].as<UUID>(UUID::EMPTY));
                text.Color = node["Color"].as<glm::vec4>(glm::vec4(1.0f));
                text.Size = node["Size"].as<float>(0.0f);
                text.AutoSize = node["AutoSize"].as<bool>(false);
                const bool hasAutoSizeRange = node["AutoSizeMin"] || node["AutoSizeMax"];
                text.AutoSizeMin = node["AutoSizeMin"].as<float>(hasAutoSizeRange ? 8.0f : 36.0f);
                text.AutoSizeMax = node["AutoSizeMax"].as<float>(hasAutoSizeRange ? 72.0f : 36.0f);
                text.AutoSizeMin = std::max(0.0f, text.AutoSizeMin);
                text.AutoSizeMax = std::max(text.AutoSizeMin, text.AutoSizeMax);
                text.LayoutSize = glm::max(node["LayoutSize"].as<glm::vec2>(glm::vec2(0.0f)), glm::vec2(0.0f));
                text.Wrapping = node["Wrapping"].as<bool>(false);
                DeserializeEnumYAML(node, "WrapMode", text.WrapMode, TextWrapMode::WordThenCharacter);
                text.ClipToBounds = node["ClipToBounds"].as<bool>(false);
                text.MaxLines = node["MaxLines"].as<uint32_t>(0);
                text.FontStyle = static_cast<TextFontStyleBits>(node["FontStyle"].as<uint32_t>(0));
                text.OutlineColor = node["OutlineColor"].as<glm::vec4>(glm::vec4(0.0f));
                text.Thickness = node["Thickness"].as<float>(0.8f);
                text.ShadowColor = node["ShadowColor"].as<glm::vec4>(glm::vec4(0.0f));
                text.ShadowOffset = node["ShadowOffset"].as<glm::vec2>(glm::vec2(1.0f, -1.0f));
                text.ShadowSoftness = std::max(0.0f, node["ShadowSoftness"].as<float>(0.0f));
                text.CharacterSpacing = node["CharacterSpacing"].as<float>(0.0f);
                text.WordSpacing = node["WordSpacing"].as<float>(0.0f);
                text.LineSpacing = node["LineSpacing"].as<float>(0.0f);
                text.ParagraphSpacing = node["ParagraphSpacing"].as<float>(0.0f);
                text.TabWidth = std::max(1u, node["TabWidth"].as<uint32_t>(4));
                text.UseCustomDecorationColor = node["UseCustomDecorationColor"].as<bool>(false);
                text.DecorationColor = node["DecorationColor"].as<glm::vec4>(glm::vec4(1.0f));
                text.DecorationThickness = std::max(0.0f, node["DecorationThickness"].as<float>(0.0f));
                text.UnderlineOffset = node["UnderlineOffset"].as<float>(0.0f);
                text.StrikethroughOffset = node["StrikethroughOffset"].as<float>(0.0f);
                text.UseKerning = node["UseKerning"].as<bool>(true);
                DeserializeEnumYAML(node, "Overflow", text.Overflow, TextOverflow::Overflow);
                DeserializeEnumYAML(node, "HorizontalAlignment", text.HorizontalAlignment, TextHorizontalAlignment::Left);
                DeserializeEnumYAML(node, "VerticalAlignment", text.VerticalAlignment, TextVerticalAlignment::Top);
                text.SortingLayer = node["SortingLayer"].as<int32_t>(0);
                text.OrderInLayer = node["OrderInLayer"].as<int32_t>(0);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& text = entity.GetComponent<TextComponent>();
                archive(text.Text, text.Font.GetUUID());
                archive(text.Color.x, text.Color.y, text.Color.z, text.Color.w);
                archive(text.Size, text.AutoSize, text.Wrapping);
                archive(text.OutlineColor.x, text.OutlineColor.y, text.OutlineColor.z, text.OutlineColor.w);
                archive(text.Thickness, text.CharacterSpacing, text.WordSpacing, text.LineSpacing, text.UseKerning);
                archive(static_cast<uint32_t>(text.FontStyle), static_cast<uint32_t>(text.Overflow), static_cast<uint32_t>(text.HorizontalAlignment),
                        static_cast<uint32_t>(text.VerticalAlignment));
                archive(text.AutoSizeMin, text.AutoSizeMax, text.LayoutSize.x, text.LayoutSize.y);
                archive(static_cast<uint32_t>(text.WrapMode), text.ClipToBounds, text.MaxLines, text.ParagraphSpacing);
                archive(text.UseCustomDecorationColor, text.DecorationColor.x, text.DecorationColor.y, text.DecorationColor.z,
                        text.DecorationColor.w);
                archive(text.DecorationThickness, text.UnderlineOffset, text.StrikethroughOffset);
                archive(text.SortingLayer, text.OrderInLayer);
                archive(text.TabWidth, text.ShadowColor.x, text.ShadowColor.y, text.ShadowColor.z, text.ShadowColor.w);
                archive(text.ShadowOffset.x, text.ShadowOffset.y, text.ShadowSoftness);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& text = entity.AddComponent<TextComponent>();
                UUID font;
                archive(text.Text, font);
                text.Font = LoadAssetReference<Font>(font);
                archive(text.Color.x, text.Color.y, text.Color.z, text.Color.w);
                archive(text.Size, text.AutoSize, text.Wrapping);
                archive(text.OutlineColor.x, text.OutlineColor.y, text.OutlineColor.z, text.OutlineColor.w);
                archive(text.Thickness, text.CharacterSpacing, text.WordSpacing, text.LineSpacing, text.UseKerning);
                uint32_t style, overflow, horizontalAlignment, verticalAlignment;
                archive(style, overflow, horizontalAlignment, verticalAlignment);
                text.FontStyle = static_cast<TextFontStyleBits>(style);
                text.Overflow = static_cast<TextOverflow>(overflow);
                text.HorizontalAlignment = static_cast<TextHorizontalAlignment>(horizontalAlignment);
                text.VerticalAlignment = static_cast<TextVerticalAlignment>(verticalAlignment);
                archive(text.AutoSizeMin, text.AutoSizeMax, text.LayoutSize.x, text.LayoutSize.y);
                uint32_t wrapMode;
                archive(wrapMode, text.ClipToBounds, text.MaxLines, text.ParagraphSpacing);
                text.WrapMode = static_cast<TextWrapMode>(wrapMode);
                archive(text.UseCustomDecorationColor, text.DecorationColor.x, text.DecorationColor.y, text.DecorationColor.z,
                        text.DecorationColor.w);
                archive(text.DecorationThickness, text.UnderlineOffset, text.StrikethroughOffset);
                archive(text.SortingLayer, text.OrderInLayer);
                archive(text.TabWidth, text.ShadowColor.x, text.ShadowColor.y, text.ShadowColor.z, text.ShadowColor.w);
                archive(text.ShadowOffset.x, text.ShadowOffset.y, text.ShadowSoftness);
            }
        };

        template <> struct ComponentIO<AudioListenerComponent>
        {
            static void WriteYaml(YAML::Emitter&, Entity) {}
            static void ReadYaml(const YAML::Node&, Entity entity, SceneComponentReadContext&) { entity.AddComponent<AudioListenerComponent>(); }
            static void WriteBinary(BinaryDataStreamOutputArchive&, Entity) {}
            static void ReadBinary(BinaryDataStreamInputArchive&, Entity entity, SceneComponentReadContext&)
            {
                entity.AddComponent<AudioListenerComponent>();
            }
        };

        template <> struct ComponentIO<AudioSourceComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& source = entity.GetComponent<AudioSourceComponent>();
                SerializeValueYAML(out, "AudioClip", source.GetClip().GetUUID());
                SerializeValueYAML(out, "Volume", source.GetVolume());
                SerializeValueYAML(out, "Pitch", source.GetPitch());
                SerializeValueYAML(out, "Loop", source.GetLooping());
                SerializeValueYAML(out, "MinDistance", source.GetMinDistance());
                SerializeValueYAML(out, "MaxDistance", source.GetMaxDistance());
                SerializeValueYAML(out, "PlayOnAwake", source.GetPlayOnAwake());
                SerializeValueYAML(out, "Muted", source.GetIsMuted());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& source = entity.AddComponent<AudioSourceComponent>();
                source.SetPlayOnAwake(node["PlayOnAwake"].as<bool>());
                source.SetVolume(node["Volume"].as<float>());
                source.SetPitch(node["Pitch"].as<float>());
                source.SetMinDistance(node["MinDistance"].as<float>());
                source.SetMaxDistance(node["MaxDistance"].as<float>());
                source.SetLooping(node["Loop"].as<bool>());
                source.SetIsMuted(node["Muted"].as<bool>(false));
                source.SetClip(LoadAssetReference<AudioClip>(node["AudioClip"].as<UUID>(UUID::EMPTY)));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& source = entity.GetComponent<AudioSourceComponent>();
                archive(source.GetClip().GetUUID());
                archive(source.GetVolume(), source.GetPitch(), source.GetLooping());
                archive(source.GetMinDistance(), source.GetMaxDistance(), source.GetPlayOnAwake(), source.GetIsMuted());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& source = entity.AddComponent<AudioSourceComponent>();
                UUID clip;
                float volume, pitch, minDistance, maxDistance;
                bool looping, playOnAwake, muted;
                archive(clip);
                archive(volume, pitch, looping, minDistance, maxDistance, playOnAwake, muted);
                source.SetClip(LoadAssetReference<AudioClip>(clip));
                source.SetVolume(volume);
                source.SetPitch(pitch);
                source.SetLooping(looping);
                source.SetMinDistance(minDistance);
                source.SetMaxDistance(maxDistance);
                source.SetPlayOnAwake(playOnAwake);
                source.SetIsMuted(muted);
            }
        };

        template <> struct ComponentIO<ManagedScriptComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                auto& scripts = entity.GetComponent<ManagedScriptComponent>().Scripts;
                SerializeValueYAML(out, "Scripts", YAML::BeginSeq);
                for (auto& script : scripts)
                {
                    const ScriptState state = ScriptRuntime::CaptureState(script);
                    out << YAML::BeginMap;
                    SerializeValueYAML(out, "State", WriteManagedStateJson(state));
                    out << YAML::EndMap;
                }
                EndYAMLSeq(out);
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext& context)
            {
                const YAML::Node entries = node["Scripts"];
                if (!entries || !entries.IsSequence())
                {
                    CW_ENGINE_WARN("Entity '{}' has a malformed managed script list. The entries were ignored.", entity.GetName());
                    return;
                }
                for (const YAML::Node& entry : entries)
                {
                    try
                    {
                        const String encoded = entry["State"].as<String>("");
                        ScriptState state;
                        ManagedOperationResult parsed = ParseManagedStateJson(encoded, state, ManagedBackendId::GeneratedMetadata);
                        if (!parsed.Succeeded)
                        {
                            CW_ENGINE_WARN("Entity '{}' contains malformed managed script state. The entry was ignored.", entity.GetName());
                            continue;
                        }
                        context.TargetScene->AddScriptComponent(entity, state);
                    }
                    catch (const std::exception& exception)
                    {
                        CW_ENGINE_WARN("Could not read managed script state on entity '{}'. {}.", entity.GetName(), exception.what());
                    }
                }
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                auto& scripts = entity.GetComponent<ManagedScriptComponent>().Scripts;
                archive(static_cast<uint32_t>(scripts.size()));
                for (auto& script : scripts)
                {
                    const String state = WriteManagedStateJson(ScriptRuntime::CaptureState(script));
                    archive(state);
                }
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext& context)
            {
                uint32_t scriptCount;
                archive(scriptCount);
                for (uint32_t index = 0; index < scriptCount; index++)
                {
                    String encoded;
                    archive(encoded);
                    ScriptState state;
                    ManagedOperationResult parsed = ParseManagedStateJson(encoded, state, ManagedBackendId::GeneratedMetadata);
                    if (!parsed.Succeeded)
                    {
                        CW_ENGINE_WARN("Entity '{}' contains malformed managed script state. The entry was ignored.", entity.GetName());
                        continue;
                    }
                    context.TargetScene->AddScriptComponent(entity, state);
                }
            }
        };

        template <> struct ComponentIO<Rigidbody2DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                SerializeEnumYAML(out, "BodyType", rigidbody.GetBodyType());
                SerializeValueYAML(out, "Mass", rigidbody.GetMass());
                SerializeValueYAML(out, "GravityScale", rigidbody.GetGravityScale());
                SerializeFlagsYAML(out, "Constraints", rigidbody.GetConstraints());
                SerializeEnumYAML(out, "CollisionDetectionMode", rigidbody.GetCollisionDetectionMode());
                SerializeEnumYAML(out, "SleepMode", rigidbody.GetSleepMode());
                SerializeValueYAML(out, "LinearDrag", rigidbody.GetLinearDrag());
                SerializeValueYAML(out, "AngularDrag", rigidbody.GetAngularDrag());
                SerializeValueYAML(out, "LayerMask", rigidbody.GetLayerMask());
                SerializeValueYAML(out, "AutoMass", rigidbody.GetAutoMass());
                SerializeEnumYAML(out, "Interpolation", rigidbody.GetInterpolationMode());
                SerializeValueYAML(out, "CenterOfMass", rigidbody.GetConfiguredCenterOfMass());
                SerializeValueYAML(out, "Inertia", rigidbody.GetConfiguredInertia());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& rigidbody = entity.AddComponent<Rigidbody2DComponent>();
                rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(node["BodyType"].as<uint32_t>()));
                rigidbody.SetMass(node["Mass"].as<float>());
                rigidbody.SetGravityScale(node["GravityScale"].as<float>());
                rigidbody.SetLayerMask(node["LayerMask"].as<uint32_t>(0), entity);
                rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(node["CollisionDetectionMode"].as<uint32_t>(0)));
                rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(node["SleepMode"].as<uint32_t>(1)));
                rigidbody.SetLinearDrag(node["LinearDrag"].as<float>(0.0f));
                rigidbody.SetAngularDrag(node["AngularDrag"].as<float>(0.05f));
                rigidbody.SetConstraints(static_cast<Rigidbody2DConstraints>(node["Constraints"].as<uint32_t>()));
                rigidbody.SetAutoMass(node["AutoMass"].as<bool>(false), entity);
                rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(node["Interpolation"].as<uint32_t>()));
                rigidbody.SetCenterOfMass(node["CenterOfMass"].as<glm::vec2>(glm::vec2(0.0f)));
                rigidbody.SetInertia(node["Inertia"].as<float>(0.0f));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                archive(static_cast<uint32_t>(rigidbody.GetBodyType()));
                archive(rigidbody.GetMass(), rigidbody.GetGravityScale(), static_cast<uint32_t>(rigidbody.GetConstraints()));
                archive(static_cast<uint32_t>(rigidbody.GetCollisionDetectionMode()), static_cast<uint32_t>(rigidbody.GetSleepMode()));
                archive(rigidbody.GetLinearDrag(), rigidbody.GetAngularDrag(), rigidbody.GetLayerMask());
                archive(rigidbody.GetAutoMass(), static_cast<uint32_t>(rigidbody.GetInterpolationMode()));
                archive(rigidbody.GetConfiguredCenterOfMass().x, rigidbody.GetConfiguredCenterOfMass().y, rigidbody.GetConfiguredInertia());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& rigidbody = entity.AddComponent<Rigidbody2DComponent>();
                uint32_t bodyType, constraints, collisionMode, sleepMode, interpolation;
                float mass, gravityScale, linearDrag, angularDrag;
                uint32_t layerMask;
                bool autoMass;
                archive(bodyType, mass, gravityScale, constraints, collisionMode, sleepMode, linearDrag, angularDrag, layerMask, autoMass,
                        interpolation);
                rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(bodyType));
                rigidbody.SetMass(mass);
                rigidbody.SetGravityScale(gravityScale);
                rigidbody.SetConstraints(static_cast<Rigidbody2DConstraints>(constraints));
                rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(collisionMode));
                rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(sleepMode));
                rigidbody.SetLinearDrag(linearDrag);
                rigidbody.SetAngularDrag(angularDrag);
                rigidbody.SetLayerMask(layerMask, entity);
                rigidbody.SetAutoMass(autoMass, entity);
                rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(interpolation));
                glm::vec2 center;
                float inertia;
                archive(center.x, center.y, inertia);
                rigidbody.SetCenterOfMass(center);
                rigidbody.SetInertia(inertia);
            }
        };

        template <> struct ComponentIO<BoxCollider2DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
                SerializeValueYAML(out, "Offset", collider.GetOffset());
                SerializeValueYAML(out, "Size", collider.GetSize());
                SerializeValueYAML(out, "IsTrigger", collider.IsTrigger());
                WritePhysicsMaterialYaml(out, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<BoxCollider2DComponent>();
                collider.SetOffset(node["Offset"].as<glm::vec2>(), entity);
                collider.SetSize(node["Size"].as<glm::vec2>(), entity);
                collider.SetIsTrigger(node["IsTrigger"].as<bool>());
                if (node["Material"])
                    collider.SetMaterial(LoadAssetReference<PhysicsMaterial2D>(node["Material"].as<UUID>(UUID::EMPTY)));
                collider.SetMaterialOverride(ReadPhysicsMaterialOverrideYaml(node));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
                archive(collider.GetOffset().x, collider.GetOffset().y);
                archive(collider.GetSize().x, collider.GetSize().y, collider.IsTrigger());
                WritePhysicsMaterialBinary(archive, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<BoxCollider2DComponent>();
                glm::vec2 offset, size;
                bool trigger;
                archive(offset.x, offset.y, size.x, size.y, trigger);
                const PhysicsMaterialBinding<PhysicsMaterial2D> material = ReadPhysicsMaterialBinary<PhysicsMaterial2D>(archive);
                collider.SetOffset(offset, entity);
                collider.SetSize(size, entity);
                collider.SetIsTrigger(trigger);
                if (material.Material.HasUUID())
                    collider.SetMaterial(material.Material);
                collider.SetMaterialOverride(material.Override);
            }
        };

        template <> struct ComponentIO<CircleCollider2DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
                SerializeValueYAML(out, "Offset", collider.GetOffset());
                SerializeValueYAML(out, "Size", collider.GetRadius());
                SerializeValueYAML(out, "IsTrigger", collider.IsTrigger());
                WritePhysicsMaterialYaml(out, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<CircleCollider2DComponent>();
                collider.SetOffset(node["Offset"].as<glm::vec2>(), entity);
                collider.SetRadius(node["Size"].as<float>(), entity);
                collider.SetIsTrigger(node["IsTrigger"].as<bool>());
                if (node["Material"])
                    collider.SetMaterial(LoadAssetReference<PhysicsMaterial2D>(node["Material"].as<UUID>(UUID::EMPTY)));
                collider.SetMaterialOverride(ReadPhysicsMaterialOverrideYaml(node));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
                archive(collider.GetOffset().x, collider.GetOffset().y, collider.GetRadius(), collider.IsTrigger());
                WritePhysicsMaterialBinary(archive, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<CircleCollider2DComponent>();
                glm::vec2 offset;
                float radius;
                bool trigger;
                archive(offset.x, offset.y, radius, trigger);
                const PhysicsMaterialBinding<PhysicsMaterial2D> material = ReadPhysicsMaterialBinary<PhysicsMaterial2D>(archive);
                collider.SetOffset(offset, entity);
                collider.SetRadius(radius, entity);
                collider.SetIsTrigger(trigger);
                if (material.Material.HasUUID())
                    collider.SetMaterial(material.Material);
                collider.SetMaterialOverride(material.Override);
            }
        };

        template <> struct ComponentIO<RelationshipComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& relationship = entity.GetComponent<RelationshipComponent>();
                SerializeValueYAML(out, "Children", YAML::BeginSeq);
                for (Entity child : relationship.Children)
                    out << child.GetUuid();
                out << YAML::EndSeq;
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext& context)
            {
                Vector<UUID> children;
                const YAML::Node& childNodes = node["Children"];
                if (childNodes && childNodes.IsSequence())
                {
                    children.reserve(childNodes.size());
                    for (const auto& child : childNodes)
                        children.push_back(child.as<UUID>());
                }
                (*context.Relationships)[entity] = std::move(children);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& relationship = entity.GetComponent<RelationshipComponent>();
                archive(static_cast<uint32_t>(relationship.Children.size()));
                for (Entity child : relationship.Children)
                    archive(child.GetUuid());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext& context)
            {
                uint32_t childCount;
                archive(childCount);
                Vector<UUID> children;
                children.reserve(childCount);
                for (uint32_t index = 0; index < childCount; index++)
                {
                    UUID child;
                    archive(child);
                    children.push_back(child);
                }
                (*context.Relationships)[entity] = std::move(children);
            }
        };

        template <> struct ComponentIO<PrefabComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& prefab = entity.GetComponent<PrefabComponent>();
                SerializeValueYAML(out, "PrefabAsset", prefab.PrefabAssetUuid);
                SerializeValueYAML(out, "PrefabEntity", prefab.PrefabEntityUuid);
                out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginSeq;
                for (const auto& overridePath : prefab.Overrides)
                    out << overridePath;
                out << YAML::EndSeq;
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& prefab = entity.AddComponent<PrefabComponent>();
                prefab.PrefabAssetUuid = node["PrefabAsset"].as<UUID>(UUID::EMPTY);
                prefab.PrefabEntityUuid = node["PrefabEntity"].as<UUID>(UUID::EMPTY);
                const YAML::Node& overrides = node["Overrides"];
                if (overrides && overrides.IsSequence())
                {
                    for (const auto& overridePath : overrides)
                        prefab.Overrides.insert(overridePath.as<String>());
                }
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& prefab = entity.GetComponent<PrefabComponent>();
                archive(prefab.PrefabAssetUuid, prefab.PrefabEntityUuid, static_cast<uint32_t>(prefab.Overrides.size()));
                for (const auto& overridePath : prefab.Overrides)
                    archive(overridePath);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& prefab = entity.AddComponent<PrefabComponent>();
                uint32_t overrideCount;
                archive(prefab.PrefabAssetUuid, prefab.PrefabEntityUuid, overrideCount);
                for (uint32_t index = 0; index < overrideCount; index++)
                {
                    String overridePath;
                    archive(overridePath);
                    prefab.Overrides.insert(std::move(overridePath));
                }
            }
        };

        template <> struct ComponentIO<ProceduralMeshComponent>
        {
            static PinDataType GetPinType(const ProceduralMeshComponent& mesh, const UUID& id)
            {
                if (mesh.Graph)
                {
                    if (const auto* input = mesh.Graph->GetGraph()->GetInput(id))
                        return input->DataType;
                }
                return PinDataType::Float;
            }

            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& mesh = entity.GetComponent<ProceduralMeshComponent>();
                SerializeValueYAML(out, "Graph", mesh.Graph.GetUUID());
                out << YAML::Key << "InputValues" << YAML::Value << YAML::BeginSeq;
                for (const auto& [id, value] : mesh.InputValues)
                {
                    out << YAML::BeginMap;
                    SerializeValueYAML(out, "ID", id);
                    const PinDataType type = GetPinType(mesh, id);
                    SerializeEnumYAML(out, "Type", type);
                    out << YAML::Key << "Value" << YAML::Value;
                    switch (type)
                    {
                    case PinDataType::Float:
                        out << std::get<float>(value);
                        break;
                    case PinDataType::Int:
                        out << std::get<int32_t>(value);
                        break;
                    case PinDataType::Vec3:
                        out << std::get<glm::vec3>(value);
                        break;
                    case PinDataType::Bool:
                        out << std::get<bool>(value);
                        break;
                    default:
                        out << 0.0f;
                        break;
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
                out << YAML::Key << "Materials" << YAML::Value << YAML::BeginSeq;
                for (const auto& material : mesh.Materials)
                    out << material.GetUUID();
                out << YAML::EndSeq;
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& mesh = entity.AddComponent<ProceduralMeshComponent>();
                const UUID graph = node["Graph"].as<UUID>(UUID::EMPTY);
                mesh.Graph = LoadAssetReference<NodeGraphAsset>(graph);
                const YAML::Node& inputs = node["InputValues"];
                if (inputs && inputs.IsSequence())
                {
                    for (const auto& input : inputs)
                    {
                        const UUID id = input["ID"].as<UUID>();
                        PinDataType type;
                        DeserializeEnumYAML(input, "Type", type, PinDataType::Float);
                        PinValue value = DefaultPinValue(type);
                        ReadPinValueYaml(input["Value"], type, value);
                        mesh.InputValues[id] = value;
                    }
                }
                const YAML::Node& materials = node["Materials"];
                if (materials && materials.IsSequence())
                {
                    for (const auto& material : materials)
                        mesh.Materials.push_back(LoadAssetReference<Material>(material.as<UUID>(UUID::EMPTY)));
                }
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& mesh = entity.GetComponent<ProceduralMeshComponent>();
                archive(mesh.Graph.GetUUID(), static_cast<uint32_t>(mesh.InputValues.size()));
                for (const auto& [id, value] : mesh.InputValues)
                {
                    const PinDataType type = GetPinType(mesh, id);
                    archive(id, static_cast<uint8_t>(type));
                    switch (type)
                    {
                    case PinDataType::Float:
                        archive(std::get<float>(value));
                        break;
                    case PinDataType::Int:
                        archive(std::get<int32_t>(value));
                        break;
                    case PinDataType::Vec3: {
                        const glm::vec3 vector = std::get<glm::vec3>(value);
                        archive(vector.x, vector.y, vector.z);
                        break;
                    }
                    case PinDataType::Bool:
                        archive(std::get<bool>(value));
                        break;
                    default:
                        archive(0.0f);
                        break;
                    }
                }
                archive(static_cast<uint32_t>(mesh.Materials.size()));
                for (const auto& material : mesh.Materials)
                    archive(material.GetUUID());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& mesh = entity.AddComponent<ProceduralMeshComponent>();
                UUID graph;
                uint32_t inputCount;
                archive(graph, inputCount);
                mesh.Graph = LoadAssetReference<NodeGraphAsset>(graph);
                for (uint32_t index = 0; index < inputCount; index++)
                {
                    UUID id;
                    uint8_t rawType;
                    archive(id, rawType);
                    const PinDataType type = static_cast<PinDataType>(rawType);
                    PinValue value = DefaultPinValue(type);
                    switch (type)
                    {
                    case PinDataType::Float: {
                        float scalar;
                        archive(scalar);
                        value = scalar;
                        break;
                    }
                    case PinDataType::Int: {
                        int32_t scalar;
                        archive(scalar);
                        value = scalar;
                        break;
                    }
                    case PinDataType::Vec3: {
                        glm::vec3 vector;
                        archive(vector.x, vector.y, vector.z);
                        value = vector;
                        break;
                    }
                    case PinDataType::Bool: {
                        bool scalar;
                        archive(scalar);
                        value = scalar;
                        break;
                    }
                    default:
                        break;
                    }
                    mesh.InputValues[id] = value;
                }
                uint32_t materialCount;
                archive(materialCount);
                mesh.Materials.reserve(materialCount);
                for (uint32_t index = 0; index < materialCount; index++)
                {
                    UUID material;
                    archive(material);
                    mesh.Materials.push_back(LoadAssetReference<Material>(material));
                }
            }
        };

        template <> struct ComponentIO<Rigidbody3DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& rigidbody = entity.GetComponent<Rigidbody3DComponent>();
                SerializeEnumYAML(out, "BodyType", rigidbody.GetBodyType());
                SerializeValueYAML(out, "Mass", rigidbody.GetMass());
                SerializeValueYAML(out, "AutoMass", rigidbody.GetAutoMass());
                SerializeValueYAML(out, "GravityScale", rigidbody.GetGravityScale());
                SerializeValueYAML(out, "LinearDamping", rigidbody.GetLinearDamping());
                SerializeValueYAML(out, "AngularDamping", rigidbody.GetAngularDamping());
                SerializeValueYAML(out, "CenterOfMass", rigidbody.GetCenterOfMass());
                SerializeValueYAML(out, "AllowSleep", rigidbody.GetAllowSleep());
                SerializeValueYAML(out, "StartAwake", rigidbody.GetStartAwake());
                SerializeValueYAML(out, "ContinuousCollision", rigidbody.GetContinuousCollision());
                SerializeValueYAML(out, "LockRotationX", rigidbody.GetLockRotationX());
                SerializeValueYAML(out, "LockRotationY", rigidbody.GetLockRotationY());
                SerializeValueYAML(out, "LockRotationZ", rigidbody.GetLockRotationZ());
                SerializeValueYAML(out, "Layer", rigidbody.GetFilter().Layer);
                SerializeValueYAML(out, "Mask", rigidbody.GetFilter().Mask);
                SerializeValueYAML(out, "Group", rigidbody.GetFilter().Group);
                SerializeValueYAML(out, "LinearVelocity", rigidbody.GetLinearVelocity());
                SerializeValueYAML(out, "AngularVelocity", rigidbody.GetAngularVelocity());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& rigidbody = entity.AddComponent<Rigidbody3DComponent>();
                rigidbody.SetBodyType(static_cast<PhysicsBodyType3D>(node["BodyType"].as<uint32_t>(0)), entity);
                rigidbody.SetMass(node["Mass"].as<float>(1.0f), entity);
                rigidbody.SetAutoMass(node["AutoMass"].as<bool>(true), entity);
                rigidbody.SetGravityScale(node["GravityScale"].as<float>(1.0f));
                rigidbody.SetDamping(node["LinearDamping"].as<float>(0.0f), node["AngularDamping"].as<float>(0.05f));
                rigidbody.SetCenterOfMass(node["CenterOfMass"].as<glm::vec3>(glm::vec3(0.0f)), entity);
                rigidbody.SetAllowSleep(node["AllowSleep"].as<bool>(true), entity);
                rigidbody.SetStartAwake(node["StartAwake"].as<bool>(true), entity);
                rigidbody.SetContinuousCollision(node["ContinuousCollision"].as<bool>(false), entity);
                rigidbody.SetRotationLocks(node["LockRotationX"].as<bool>(false), node["LockRotationY"].as<bool>(false),
                                           node["LockRotationZ"].as<bool>(false), entity);
                PhysicsFilter3D filter;
                filter.Layer = node["Layer"].as<uint32_t>(0);
                filter.Mask = node["Mask"].as<uint32_t>(0xffffffff);
                filter.Group = node["Group"].as<int32_t>(0);
                rigidbody.SetFilter(filter);
                rigidbody.SetLinearVelocity(node["LinearVelocity"].as<glm::vec3>(glm::vec3(0.0f)));
                rigidbody.SetAngularVelocity(node["AngularVelocity"].as<glm::vec3>(glm::vec3(0.0f)));
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& rigidbody = entity.GetComponent<Rigidbody3DComponent>();
                archive(static_cast<uint32_t>(rigidbody.GetBodyType()), rigidbody.GetMass(), rigidbody.GetAutoMass());
                archive(rigidbody.GetGravityScale(), rigidbody.GetLinearDamping(), rigidbody.GetAngularDamping());
                const glm::vec3 center = rigidbody.GetCenterOfMass();
                archive(center.x, center.y, center.z);
                archive(rigidbody.GetAllowSleep(), rigidbody.GetStartAwake(), rigidbody.GetContinuousCollision());
                archive(rigidbody.GetLockRotationX(), rigidbody.GetLockRotationY(), rigidbody.GetLockRotationZ());
                archive(rigidbody.GetFilter().Layer, rigidbody.GetFilter().Mask, rigidbody.GetFilter().Group);
                const glm::vec3 linearVelocity = rigidbody.GetLinearVelocity();
                const glm::vec3 angularVelocity = rigidbody.GetAngularVelocity();
                archive(linearVelocity.x, linearVelocity.y, linearVelocity.z);
                archive(angularVelocity.x, angularVelocity.y, angularVelocity.z);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& rigidbody = entity.AddComponent<Rigidbody3DComponent>();
                uint32_t bodyType;
                float mass, gravityScale, linearDamping, angularDamping;
                bool autoMass, allowSleep, startAwake, continuous;
                bool lockX, lockY, lockZ;
                glm::vec3 center, linearVelocity, angularVelocity;
                PhysicsFilter3D filter;
                archive(bodyType, mass, autoMass);
                archive(gravityScale, linearDamping, angularDamping);
                archive(center.x, center.y, center.z);
                archive(allowSleep, startAwake, continuous);
                archive(lockX, lockY, lockZ);
                archive(filter.Layer, filter.Mask, filter.Group);
                archive(linearVelocity.x, linearVelocity.y, linearVelocity.z);
                archive(angularVelocity.x, angularVelocity.y, angularVelocity.z);
                rigidbody.SetBodyType(static_cast<PhysicsBodyType3D>(bodyType), entity);
                rigidbody.SetMass(mass, entity);
                rigidbody.SetAutoMass(autoMass, entity);
                rigidbody.SetGravityScale(gravityScale);
                rigidbody.SetDamping(linearDamping, angularDamping);
                rigidbody.SetCenterOfMass(center, entity);
                rigidbody.SetAllowSleep(allowSleep, entity);
                rigidbody.SetStartAwake(startAwake, entity);
                rigidbody.SetContinuousCollision(continuous, entity);
                rigidbody.SetRotationLocks(lockX, lockY, lockZ, entity);
                rigidbody.SetFilter(filter);
                rigidbody.SetLinearVelocity(linearVelocity);
                rigidbody.SetAngularVelocity(angularVelocity);
            }
        };

        void WriteCollider3DYaml(YAML::Emitter& out, const Collider3D& collider)
        {
            SerializeValueYAML(out, "Offset", collider.GetOffset());
            SerializeValueYAML(out, "Rotation", collider.GetRotation());
            SerializeValueYAML(out, "IsTrigger", collider.IsTrigger());
            WritePhysicsMaterialYaml(out, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            SerializeValueYAML(out, "Layer", collider.GetFilter().Layer);
            SerializeValueYAML(out, "Mask", collider.GetFilter().Mask);
            SerializeValueYAML(out, "Group", collider.GetFilter().Group);
        }

        void ReadCollider3DYaml(const YAML::Node& node, Collider3D& collider, Entity entity)
        {
            collider.SetOffset(node["Offset"].as<glm::vec3>(glm::vec3(0.0f)), entity);
            collider.SetRotation(node["Rotation"].as<glm::quat>(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), entity);
            collider.SetIsTrigger(node["IsTrigger"].as<bool>(false));
            if (node["Material"])
                collider.SetMaterial(LoadAssetReference<PhysicsMaterial3D>(node["Material"].as<UUID>(UUID::EMPTY)));
            collider.SetMaterialOverride(ReadPhysicsMaterialOverrideYaml(node));
            PhysicsFilter3D filter;
            filter.Layer = node["Layer"].as<uint32_t>(0);
            filter.Mask = node["Mask"].as<uint32_t>(0xffffffff);
            filter.Group = node["Group"].as<int32_t>(0);
            collider.SetFilter(filter, entity);
        }

        void WriteCollider3DBinary(BinaryDataStreamOutputArchive& archive, const Collider3D& collider)
        {
            const glm::vec3 offset = collider.GetOffset();
            const glm::quat rotation = collider.GetRotation();
            archive(offset.x, offset.y, offset.z);
            archive(rotation.x, rotation.y, rotation.z, rotation.w);
            archive(collider.IsTrigger());
            WritePhysicsMaterialBinary(archive, collider.GetMaterial(), collider.GetMaterialOverride(), collider.GetMaterialData());
            archive(collider.GetFilter().Layer, collider.GetFilter().Mask, collider.GetFilter().Group);
        }

        void ReadCollider3DBinary(BinaryDataStreamInputArchive& archive, Collider3D& collider, Entity entity)
        {
            glm::vec3 offset;
            glm::quat rotation;
            bool trigger;
            PhysicsFilter3D filter;
            archive(offset.x, offset.y, offset.z);
            archive(rotation.x, rotation.y, rotation.z, rotation.w);
            archive(trigger);
            const PhysicsMaterialBinding<PhysicsMaterial3D> material = ReadPhysicsMaterialBinary<PhysicsMaterial3D>(archive);
            archive(filter.Layer, filter.Mask, filter.Group);
            collider.SetOffset(offset, entity);
            collider.SetRotation(rotation, entity);
            collider.SetIsTrigger(trigger);
            if (material.Material.HasUUID())
                collider.SetMaterial(material.Material);
            collider.SetMaterialOverride(material.Override);
            collider.SetFilter(filter, entity);
        }

        template <> struct ComponentIO<BoxCollider3DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& collider = entity.GetComponent<BoxCollider3DComponent>();
                WriteCollider3DYaml(out, collider);
                SerializeValueYAML(out, "Size", collider.GetSize());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<BoxCollider3DComponent>();
                ReadCollider3DYaml(node, collider, entity);
                collider.SetSize(node["Size"].as<glm::vec3>(glm::vec3(1.0f)), entity);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& collider = entity.GetComponent<BoxCollider3DComponent>();
                WriteCollider3DBinary(archive, collider);
                archive(collider.GetSize().x, collider.GetSize().y, collider.GetSize().z);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<BoxCollider3DComponent>();
                ReadCollider3DBinary(archive, collider, entity);
                glm::vec3 size;
                archive(size.x, size.y, size.z);
                collider.SetSize(size, entity);
            }
        };

        template <> struct ComponentIO<SphereCollider3DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& collider = entity.GetComponent<SphereCollider3DComponent>();
                WriteCollider3DYaml(out, collider);
                SerializeValueYAML(out, "Radius", collider.GetRadius());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<SphereCollider3DComponent>();
                ReadCollider3DYaml(node, collider, entity);
                collider.SetRadius(node["Radius"].as<float>(0.5f), entity);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& collider = entity.GetComponent<SphereCollider3DComponent>();
                WriteCollider3DBinary(archive, collider);
                archive(collider.GetRadius());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<SphereCollider3DComponent>();
                ReadCollider3DBinary(archive, collider, entity);
                float radius;
                archive(radius);
                collider.SetRadius(radius, entity);
            }
        };

        template <> struct ComponentIO<CapsuleCollider3DComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& collider = entity.GetComponent<CapsuleCollider3DComponent>();
                WriteCollider3DYaml(out, collider);
                SerializeValueYAML(out, "Radius", collider.GetRadius());
                SerializeValueYAML(out, "Height", collider.GetHeight());
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<CapsuleCollider3DComponent>();
                ReadCollider3DYaml(node, collider, entity);
                collider.SetRadius(node["Radius"].as<float>(0.5f), entity);
                collider.SetHeight(node["Height"].as<float>(2.0f), entity);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& collider = entity.GetComponent<CapsuleCollider3DComponent>();
                WriteCollider3DBinary(archive, collider);
                archive(collider.GetRadius(), collider.GetHeight());
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& collider = entity.AddComponent<CapsuleCollider3DComponent>();
                ReadCollider3DBinary(archive, collider, entity);
                float radius, height;
                archive(radius, height);
                collider.SetRadius(radius, entity);
                collider.SetHeight(height, entity);
            }
        };

        template <> struct ComponentIO<AnimationComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& animation = entity.GetComponent<AnimationComponent>();
                SerializeValueYAML(out, "Clip", animation.Clip.GetUUID());
                SerializeValueYAML(out, "Speed", animation.Speed);
                SerializeEnumYAML(out, "WrapMode", animation.WrapMode);
                SerializeValueYAML(out, "PlayOnAwake", animation.PlayOnAwake);
                SerializeValueYAML(out, "ApplyRootMotion", animation.ApplyRootMotion);
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& animation = entity.AddComponent<AnimationComponent>();
                animation.Clip = LoadAssetReference<AnimationClip>(node["Clip"].as<UUID>(UUID::EMPTY));
                animation.Speed = node["Speed"].as<float>(1.0f);
                DeserializeEnumYAML(node, "WrapMode", animation.WrapMode, AnimationWrapMode::Loop);
                animation.PlayOnAwake = node["PlayOnAwake"].as<bool>(true);
                animation.ApplyRootMotion = node["ApplyRootMotion"].as<bool>(false);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& animation = entity.GetComponent<AnimationComponent>();
                archive(animation.Clip.GetUUID(), animation.Speed, static_cast<uint8_t>(animation.WrapMode), animation.PlayOnAwake,
                        animation.ApplyRootMotion);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& animation = entity.AddComponent<AnimationComponent>();
                UUID clip;
                uint8_t wrapMode = static_cast<uint8_t>(AnimationWrapMode::Loop);
                archive(clip, animation.Speed, wrapMode, animation.PlayOnAwake, animation.ApplyRootMotion);
                animation.Clip = LoadAssetReference<AnimationClip>(clip);
                animation.WrapMode = static_cast<AnimationWrapMode>(wrapMode);
            }
        };

        template <> struct ComponentIO<LightComponent>
        {
            static void WriteYaml(YAML::Emitter& out, Entity entity)
            {
                const auto& light = entity.GetComponent<LightComponent>();
                SerializeEnumYAML(out, "Type", light.Type);
                SerializeValueYAML(out, "Color", light.Color);
                SerializeValueYAML(out, "Intensity", light.Intensity);
                SerializeValueYAML(out, "Range", light.Range);
                SerializeValueYAML(out, "SpotInnerAngle", light.SpotInnerAngle);
                SerializeValueYAML(out, "SpotOuterAngle", light.SpotOuterAngle);
                SerializeValueYAML(out, "SourceRadius", light.SourceRadius);
                SerializeValueYAML(out, "UseColorTemperature", light.UseColorTemperature);
                SerializeValueYAML(out, "Temperature", light.Temperature);
                SerializeValueYAML(out, "VisibilityLayers", light.VisibilityLayers.Value);
                SerializeValueYAML(out, "Enabled", light.Enabled);
                SerializeValueYAML(out, "AffectDiffuse", light.AffectDiffuse);
                SerializeValueYAML(out, "AffectSpecular", light.AffectSpecular);
                SerializeValueYAML(out, "Volumetric", light.Volumetric);
                SerializeEnumYAML(out, "ShadowMode", light.Shadows.Mode);
                SerializeValueYAML(out, "ShadowBias", light.Shadows.Bias);
                SerializeValueYAML(out, "ShadowNormalBias", light.Shadows.NormalBias);
                SerializeValueYAML(out, "ShadowNearPlane", light.Shadows.NearPlane);
                SerializeValueYAML(out, "ShadowImportance", light.Shadows.Importance);
                SerializeValueYAML(out, "ShadowResolution", light.Shadows.Resolution);
                SerializeValueYAML(out, "CacheStaticCasters", light.Shadows.CacheStaticCasters);
            }

            static void ReadYaml(const YAML::Node& node, Entity entity, SceneComponentReadContext&)
            {
                auto& light = entity.AddComponent<LightComponent>();
                DeserializeEnumYAML(node, "Type", light.Type, LightType::Point);
                light.Color = glm::max(node["Color"].as<glm::vec3>(glm::vec3(1.0f)), glm::vec3(0.0f));
                light.Intensity = std::max(node["Intensity"].as<float>(1000.0f), 0.0f);
                light.Range = std::max(node["Range"].as<float>(10.0f), 0.001f);
                light.SpotInnerAngle = std::clamp(node["SpotInnerAngle"].as<float>(glm::radians(25.0f)), 0.0f, glm::pi<float>());
                light.SpotOuterAngle = std::clamp(node["SpotOuterAngle"].as<float>(glm::radians(35.0f)), light.SpotInnerAngle, glm::pi<float>());
                light.SourceRadius = std::max(node["SourceRadius"].as<float>(0.0f), 0.0f);
                light.UseColorTemperature = node["UseColorTemperature"].as<bool>(false);
                light.Temperature = std::clamp(node["Temperature"].as<float>(6500.0f), 1000.0f, 40000.0f);
                light.VisibilityLayers.Value = node["VisibilityLayers"].as<uint32_t>(0xffffffffu);
                light.Enabled = node["Enabled"].as<bool>(true);
                light.AffectDiffuse = node["AffectDiffuse"].as<bool>(true);
                light.AffectSpecular = node["AffectSpecular"].as<bool>(true);
                light.Volumetric = node["Volumetric"].as<bool>(false);
                DeserializeEnumYAML(node, "ShadowMode", light.Shadows.Mode, LightShadowMode::Disabled);
                light.Shadows.Bias = std::max(node["ShadowBias"].as<float>(0.001f), 0.0f);
                light.Shadows.NormalBias = std::max(node["ShadowNormalBias"].as<float>(0.02f), 0.0f);
                light.Shadows.NearPlane = std::max(node["ShadowNearPlane"].as<float>(0.05f), 0.001f);
                light.Shadows.Importance = std::max(node["ShadowImportance"].as<float>(1.0f), 0.0f);
                light.Shadows.Resolution = std::max<uint16_t>(node["ShadowResolution"].as<uint16_t>(1024), 64);
                light.Shadows.CacheStaticCasters = node["CacheStaticCasters"].as<bool>(true);
            }

            static void WriteBinary(BinaryDataStreamOutputArchive& archive, Entity entity)
            {
                const auto& light = entity.GetComponent<LightComponent>();
                archive(static_cast<uint8_t>(light.Type), light.Color.x, light.Color.y, light.Color.z, light.Intensity, light.Range,
                        light.SpotInnerAngle, light.SpotOuterAngle, light.SourceRadius, light.UseColorTemperature, light.Temperature,
                        light.VisibilityLayers.Value, light.Enabled, light.AffectDiffuse, light.AffectSpecular, light.Volumetric,
                        static_cast<uint8_t>(light.Shadows.Mode), light.Shadows.Bias, light.Shadows.NormalBias, light.Shadows.NearPlane,
                        light.Shadows.Importance, light.Shadows.Resolution, light.Shadows.CacheStaticCasters);
            }

            static void ReadBinary(BinaryDataStreamInputArchive& archive, Entity entity, SceneComponentReadContext&)
            {
                auto& light = entity.AddComponent<LightComponent>();
                uint8_t type, shadowMode;
                archive(type, light.Color.x, light.Color.y, light.Color.z, light.Intensity, light.Range, light.SpotInnerAngle, light.SpotOuterAngle,
                        light.SourceRadius, light.UseColorTemperature, light.Temperature, light.VisibilityLayers.Value, light.Enabled,
                        light.AffectDiffuse, light.AffectSpecular, light.Volumetric, shadowMode, light.Shadows.Bias, light.Shadows.NormalBias,
                        light.Shadows.NearPlane, light.Shadows.Importance, light.Shadows.Resolution, light.Shadows.CacheStaticCasters);
                light.Type = static_cast<LightType>(type);
                light.Shadows.Mode = static_cast<LightShadowMode>(shadowMode);
            }
        };

        template <typename T>
        constexpr SceneComponentCodec MakeCodec(SceneComponentId id, const char* yamlName, const char* prefabPath, const char* editorName,
                                                SceneComponentYamlType yamlType = SceneComponentYamlType::Map,
                                                SceneComponentCodec::HasComponentFunction shouldSerialize = &AlwaysSerialize<T>)
        {
            return { id,
                     yamlName,
                     yamlType,
                     &HasComponent<T>,
                     shouldSerialize,
                     &ComponentIO<T>::WriteYaml,
                     &ComponentIO<T>::ReadYaml,
                     &ComponentIO<T>::WriteBinary,
                     &ComponentIO<T>::ReadBinary,
                     prefabPath,
                     editorName };
        }

        static constexpr std::array<SceneComponentCodec, 21> COMPONENT_CODECS = {
            MakeCodec<TagComponent>(SceneComponentId::Tag, "TagComponent", nullptr, "Tag"),
            MakeCodec<TransformComponent>(SceneComponentId::Transform, "TransformComponent", "Transform", "Transform"),
            MakeCodec<CameraComponent>(SceneComponentId::Camera, "CameraComponent", "Camera", "Camera"),
            MakeCodec<SpriteRendererComponent>(SceneComponentId::SpriteRenderer, "SpriteRendererComponent", "Sprite Renderer", "Sprite Renderer"),
            MakeCodec<MeshRendererComponent>(SceneComponentId::MeshRenderer, "MeshRendererComponent", "Mesh Filter", "Mesh Renderer"),
            MakeCodec<TextComponent>(SceneComponentId::Text, "TextComponent", "Text", "Text"),
            MakeCodec<AudioListenerComponent>(SceneComponentId::AudioListener, "AudioListenerComponent", nullptr, "Audio Listener",
                                              SceneComponentYamlType::Null),
            MakeCodec<AudioSourceComponent>(SceneComponentId::AudioSource, "AudioSourceComponent", "Audio Source", "Audio Source"),
            MakeCodec<ManagedScriptComponent>(SceneComponentId::ManagedScript, "ManagedScriptComponent", nullptr, "Script",
                                              SceneComponentYamlType::Map, &ShouldSerializeManagedScript),
            MakeCodec<Rigidbody2DComponent>(SceneComponentId::Rigidbody2D, "Rigidbody2DComponent", "Rigidbody 2D", "Rigidbody 2D"),
            MakeCodec<BoxCollider2DComponent>(SceneComponentId::BoxCollider2D, "BoxCollider2DComponent", "Box Collider 2D", "Box Collider 2D"),
            MakeCodec<CircleCollider2DComponent>(SceneComponentId::CircleCollider2D, "CircleCollider2DComponent", "Circle Collider 2D",
                                                 "Circle Collider 2D"),
            MakeCodec<RelationshipComponent>(SceneComponentId::Relationship, "RelationshipComponent", nullptr, nullptr),
            MakeCodec<PrefabComponent>(SceneComponentId::Prefab, "PrefabComponent", nullptr, nullptr),
            MakeCodec<ProceduralMeshComponent>(SceneComponentId::ProceduralMesh, "ProceduralMeshComponent", "Procedural Mesh", "Procedural Mesh"),
            MakeCodec<Rigidbody3DComponent>(SceneComponentId::Rigidbody3D, "Rigidbody3DComponent", "Rigidbody 3D", "Rigidbody 3D"),
            MakeCodec<BoxCollider3DComponent>(SceneComponentId::BoxCollider3D, "BoxCollider3DComponent", "Box Collider 3D", "Box Collider 3D"),
            MakeCodec<SphereCollider3DComponent>(SceneComponentId::SphereCollider3D, "SphereCollider3DComponent", "Sphere Collider 3D",
                                                 "Sphere Collider 3D"),
            MakeCodec<CapsuleCollider3DComponent>(SceneComponentId::CapsuleCollider3D, "CapsuleCollider3DComponent", "Capsule Collider 3D",
                                                  "Capsule Collider 3D"),
            MakeCodec<AnimationComponent>(SceneComponentId::Animation, "AnimationComponent", "Animation", "Animation"),
            MakeCodec<LightComponent>(SceneComponentId::Light, "LightComponent", "Light", "Light")
        };

        constexpr bool HasStableIds()
        {
            for (size_t index = 0; index < COMPONENT_CODECS.size(); index++)
            {
                if (static_cast<uint32_t>(COMPONENT_CODECS[index].Id) != index)
                    return false;
            }
            return true;
        }

        static_assert(HasStableIds(), "Scene component IDs must remain explicit and stable");
    } // namespace

    std::span<const SceneComponentCodec> GetSceneComponentCodecs() { return COMPONENT_CODECS; }

    const SceneComponentCodec* FindSceneComponentCodec(SceneComponentId id) { return FindSceneComponentCodec(static_cast<uint32_t>(id)); }

    const SceneComponentCodec* FindSceneComponentCodec(uint32_t stableId)
    {
        if (stableId >= COMPONENT_CODECS.size())
            return nullptr;
        const SceneComponentCodec& codec = COMPONENT_CODECS[stableId];
        return static_cast<uint32_t>(codec.Id) == stableId ? &codec : nullptr;
    }
} // namespace Crowny
