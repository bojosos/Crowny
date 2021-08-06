#pragma once

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Audio/OggVorbisDecoder.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/TextureManager.h"

#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h" // ?????????????????????
#include "Crowny/Scene/SceneCamera.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Common/Color.h"
#include "Crowny/Ecs/Entity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    template <class Component> void ComponentEditorWidget(Entity entity);

    struct Component
    {
        Entity ComponentParent;
        MonoObject* ManagedInstance = nullptr;
    };

    struct TagComponent : public Component
    {
        std::string Tag = "";

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}

        operator std::string &() { return Tag; }
        operator const std::string &() const { return Tag; }
    };

    struct TransformComponent : public Component
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& position) : Position(position) {}

        glm::mat4 GetTransform() const
        {
            glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

            return glm::translate(glm::mat4(1.0f), Position) * rotation * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    template <> void ComponentEditorWidget<TransformComponent>(Entity e);

    struct CameraComponent : public Component
    {
        SceneCamera Camera;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
    };

    template <> void ComponentEditorWidget<CameraComponent>(Entity e);

    struct TextComponent : public Component
    {
        std::string Text = "";
        Ref<::Crowny::Font> Font;
        glm::vec4 Color{ 0.0f, 0.3f, 0.3f, 1.0f };
        // Crowny::Material Material;

        TextComponent() { Font = FontManager::Get("default"); };
        TextComponent(const TextComponent&) = default;
        TextComponent(const std::string& text) : Text(text) {}
    };

    template <> void ComponentEditorWidget<TextComponent>(Entity e);

    struct SpriteRendererComponent : public Component
    {
        Ref<Crowny::Texture> Texture;
        glm::vec4 Color;

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const Ref<Crowny::Texture>& texture, Crowny::Color color)
          : Texture(texture), Color(color)
        {
        }
    };

    template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e);

    struct MeshRendererComponent : public Component
    {
        Ref<::Crowny::Mesh> Mesh;

        MeshRendererComponent()
        {
            Mesh = MeshFactory::CreateSphere();
            Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial()));
        };
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e);

    struct RelationshipComponent : public Component
    {
        std::vector<Entity> Children;
        Entity Parent;

        RelationshipComponent() = default;
        RelationshipComponent(const RelationshipComponent&) = default;
        RelationshipComponent(const Entity& parent) : Parent(parent) {}
    };

    struct AudioSourceComponent : public Component
    {
        Ref<AudioSource> Source;

        AudioSourceComponent()
        {
            Source = gAudio().CreateSource();
            Ref<DataStream> stream = FileSystem::OpenFile("test.ogg", true);
            Ref<OggVorbisDecoder> decoder = CreateRef<OggVorbisDecoder>();
            AudioDataInfo info;
            decoder->Open(stream, info);
            uint32_t bps = info.BitDepth / 8;
            uint32_t bufferSize = info.NumSamples * bps;
            Ref<MemoryDataStream> samples = CreateRef<MemoryDataStream>(bufferSize);
            decoder->Read(samples->Data(), info.NumSamples);

            { // covert to mono
                if (info.NumChannels > 1)
                {
                    uint32_t samplesPerChannel = info.NumSamples / info.NumChannels;
                    uint32_t monoBufferSize = samplesPerChannel * bps;
                    CW_ENGINE_INFO("{0}, {1}", info.NumSamples, info.NumChannels);
                    Ref<MemoryDataStream> monoStream = CreateRef<MemoryDataStream>(monoBufferSize);
                    CW_ENGINE_INFO(bps);
                    AudioUtils::ConvertToMono(samples->Data(), monoStream->Data(), info.BitDepth, samplesPerChannel,
                                              info.NumChannels);
                    info.NumSamples = samplesPerChannel;
                    info.NumChannels = 1;
                    samples = monoStream;
                    bufferSize = monoBufferSize;
                }
            }
            AudioClipDesc clipDesc;
            clipDesc.BitDepth = info.BitDepth;
            clipDesc.Format = AudioFormat::VORBIS;
            clipDesc.Frequency = info.SampleRate;
            clipDesc.NumChannels = info.NumChannels;
            clipDesc.ReadMode = AudioReadMode::LoadDecompressed;
            clipDesc.Is3D = true;
            Ref<AudioClip> clip = CreateRef<AudioClip>(stream, bufferSize, info.NumSamples, clipDesc);
            Source->SetClip(clip);
        }
        AudioSourceComponent(const AudioSourceComponent&) = default;
        AudioSourceComponent(const Ref<AudioSource>& source) : Source(source) {}
    };

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e);

    struct AudioListenerComponent : public Component
    {
        Ref<AudioListener> Listener;

        AudioListenerComponent() { Listener = gAudio().CreateListener(); }
        AudioListenerComponent(const AudioListenerComponent&) = default;
        AudioListenerComponent(const Ref<AudioListener>& listener) : Listener(listener) {}
    };

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e);

    struct MonoScriptComponent : public Component
    {
        CWMonoClass* Class = nullptr;
        MonoObject* Instance = nullptr;
        CWMonoMethod* OnUpdate = nullptr;
        CWMonoMethod* OnStart = nullptr;
        CWMonoMethod* OnDestroy = nullptr;
        std::vector<CWMonoField*> DisplayableFields;
        std::vector<CWMonoProperty*> DisplayableProperties;

        MonoScriptComponent() = default;
        MonoScriptComponent(const MonoScriptComponent&) = default;

        MonoScriptComponent(const std::string& name)
        {
            Class = CWMonoRuntime::GetClientAssembly()->GetClass("Sandbox", name);
            if (Class)
            {
                for (auto* field : Class->GetFields())
                {
                    bool isHidden = field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector);
                    bool isVisible = field->GetVisibility() == CWMonoVisibility::Public ||
                                     field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute) ||
                                     field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector);
                    if (field != nullptr && !isHidden && isVisible)
                        DisplayableFields.push_back(field);
                }

                for (auto* prop : Class->GetProperties())
                {
                    bool isHidden = prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector);
                    bool isVisible = prop->GetVisibility() == CWMonoVisibility::Public ||
                                     prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute) ||
                                     prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector);
                    if (prop && !isHidden && isVisible)
                        DisplayableProperties.push_back(prop);
                }
            }
        }
    };

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e);
} // namespace Crowny
