#pragma once

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioSource.h"

#include "Crowny/Common/Color.h"
#include "Crowny/Common/Uuid.h"

#include "Crowny/Physics/PhysicsMaterial.h"

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/Model.h"

#include "Crowny/Scene/SceneCamera.h"
#include "Crowny/Scene/SceneManager.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

#include "Crowny/Ecs/Entity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

class b2Body;
class b2Fixture;

namespace Crowny
{
    class ScriptEntityBehaviour;

    enum class TransformChangedFlags
    {
        None,
        Transform,
        Parent
    };

    static uint64_t s_NextAvailableId = 1;

    template <class Component> void ComponentEditorWidget(Entity entity);
    struct ComponentBase
    {
        ComponentBase()
        {
            InstanceId = s_NextAvailableId;
            s_NextAvailableId++;
        }
        uint64_t InstanceId;
    };

    struct IDComponent : public ComponentBase
    {
        UUID Uuid;

        IDComponent() : ComponentBase(){};
        IDComponent(const IDComponent&) = default;
        IDComponent(const UUID& uuid) : Uuid(uuid) {}
    };

    struct TagComponent : public ComponentBase
    {
        String Tag = "";

        TagComponent() : ComponentBase() {}
        TagComponent(const TagComponent&) = default;
        TagComponent(const String& tag) : Tag(tag) {}

        operator String&() { return Tag; }
        operator const String&() const { return Tag; }
    };

    struct TransformComponent : public ComponentBase
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        TransformComponent() : ComponentBase() {}
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& position) : Position(position) {}

        glm::mat4 GetTransform() const
        {
            glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

            return glm::translate(glm::mat4(1.0f), Position) * rotation * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    template <> void ComponentEditorWidget<TransformComponent>(Entity e);

    struct CameraComponent : public ComponentBase
    {
        SceneCamera Camera;

        CameraComponent() : ComponentBase() {}
        CameraComponent(const CameraComponent&) = default;
    };

    template <> void ComponentEditorWidget<CameraComponent>(Entity e);

    struct TextComponent : public ComponentBase
    {
        String Text = "";
        Ref<::Crowny::Font> Font = nullptr;
        glm::vec4 Color{ 0.0f, 0.3f, 0.3f, 1.0f };
        // Crowny::Material Material;

        TextComponent() : ComponentBase() { Font = FontManager::Get("default"); };
        TextComponent(const TextComponent&) = default;
        TextComponent(const String& text) : Text(text) {}
    };

    template <> void ComponentEditorWidget<TextComponent>(Entity e);

    struct SpriteRendererComponent : public ComponentBase
    {
        Ref<Crowny::Texture> Texture = nullptr;
        glm::vec4 Color = glm::vec4(1.0f);

        SpriteRendererComponent() : ComponentBase() {}
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const Ref<Crowny::Texture>& texture, Crowny::Color color)
          : Texture(texture), Color(color)
        {
        }
    };

    template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e);

    struct MeshRendererComponent : public ComponentBase
    {
        Ref<::Crowny::Mesh> Mesh = nullptr;
        // Ref<::Crowny::Model> Model = nullptr;

        MeshRendererComponent()
          : ComponentBase(){
                //    Model = CreateRef<::Crowny::Model>("Resources/Models/sphere.gltf");
            };
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e);

    struct RelationshipComponent : public ComponentBase
    {
        Vector<Entity> Children;
        Entity Parent;

        RelationshipComponent() : ComponentBase() {}
        RelationshipComponent(const RelationshipComponent& other) = default;
        RelationshipComponent(const Entity& parent) : Parent(parent) {}

        RelationshipComponent& operator=(const RelationshipComponent& other);
    };

    struct AudioSourceComponent : public ComponentBase
    {
    public:
        AudioSourceComponent() : ComponentBase() {}
        AudioSourceComponent(const AudioSourceComponent&) = default;

        void OnInitialize();
        void OnEnabled();
        void OnDisabled();
        void OnDestroyed();
        void OnTransformChanged(TransformChangedFlags flags);

        void SetVolume(float volume);
        void SetPitch(float pitch);
        void SetClip(const AssetHandle<AudioClip>& clip);
        void SetPlayOnAwake(bool playOnAwake);
        void SetMinDistance(float minDistnace);
        void SetMaxDistance(float maxDistance);
        void SetLooping(bool loop);
        void SetIsMuted(bool muted);
        void SetTime(float time);

        float GetVolume() const { return m_Volume; }
        float GetPitch() const { return m_Pitch; }
        AssetHandle<AudioClip> GetClip() const { return m_AudioClip; }
        bool GetPlayOnAwake() const { return m_PlayOnAwake; }
        float GetMinDistance() const { return m_MinDistance; }
        float GetMaxDistance() const { return m_MaxDistance; }
        bool GetLooping() const { return m_Loop; }
        bool GetIsMuted() const { return m_IsMuted; }
        float GetTime() const { return m_Time; }

        void Play();
        void Pause();
        void Stop();

        AudioSourceState GetState() const;

    private:
        AssetHandle<AudioClip> m_AudioClip;
        bool m_IsMuted = false;
        float m_Volume = 1.0f;
        float m_Pitch = 1.0f;
        bool m_Loop = false;
        float m_MinDistance = 1.0f;
        float m_MaxDistance = 500.0f;
        bool m_PlayOnAwake = true;
        float m_Time = 0.0f;

        Ref<AudioSource> m_Internal;
    };

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e);

    class AudioListenerComponent : public ComponentBase
    {
    public:
        AudioListenerComponent() = default;
        AudioListenerComponent(const AudioListenerComponent&) = default;

        void Initialize() { m_Internal = gAudio().CreateListener(); }

    private:
        Ref<AudioListener> m_Internal;
    };

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e);

    class MonoScript
    {

	public:
		MonoScript() = default;
		MonoScript(const String& name);
		MonoScript(const MonoScript&) = default;

		void SetClassName(const String & className);
		MonoClass* GetManagedClass() const;
		MonoObject* GetManagedInstance() const;

		Ref<SerializableObjectInfo> GetObjectInfo() const { return m_ObjectInfo; }

		ScriptObjectBackupData BeginRefresh();
		void EndRefresh(const ScriptObjectBackupData & data);
		
		const String& GetTypeName() const { return m_TypeName; }
		void SetTypeName(const String & typeName) { m_TypeName = typeName; }

		void OnInitialize(Entity entity);
		void OnStart();
		void OnUpdate();
		void OnDestroy();

	private:
		typedef void(CW_THUNKCALL* OnStartThunkDef)(MonoObject*, MonoException**);
		OnStartThunkDef m_OnStartThunk = nullptr;
		typedef void(CW_THUNKCALL* OnUpdateThunkDef)(MonoObject*, MonoException**);
		OnUpdateThunkDef m_OnUpdateThunk = nullptr;
		typedef void(CW_THUNKCALL* OnDestroyThunkDef)(MonoObject*, MonoException**);
		OnDestroyThunkDef m_OnDestroyThunk = nullptr;

		typedef void(CW_THUNKCALL* OnCollisionEnterThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
		OnCollisionEnterThunkDef m_OnCollisionEnterThunk = nullptr;
		typedef void(CW_THUNKCALL* OnCollisionStayThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
		OnCollisionStayThunkDef m_OnCollisionStayThunk = nullptr;
		typedef void(CW_THUNKCALL* OnCollisionExitThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
		OnCollisionExitThunkDef m_OnCollisionExitThunk = nullptr;

		String m_TypeName;
		String m_Namespace;
		bool m_MissingType = false;
		Ref<SerializableObject> m_SerializedObjectData;
		Ref<SerializableObjectInfo> m_ObjectInfo;
		MonoClass* m_Class = nullptr;
		uint32_t m_Handle = 0;
		ScriptEntityBehaviour* m_ScriptEntityBehaviour = nullptr;
    };

    class MonoScriptComponent : public ComponentBase
    {
    public:
        MonoScriptComponent() : ComponentBase()
        {
            Scripts.push_back({ });
            Scripts.back().SetTypeName(name);
        }

        MonoScriptComponent(const String& name) : ComponentBase()
        {
            Scripts.push_back({ });
			Scripts.back().SetTypeName(name);
        }
        MonoScriptComponent(const MonoScriptComponent&) = default;

		Vector<MonoScript> Scripts;
    };

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e);

    enum class Rigidbody2DConstraintsBits
    {
        None = 0,
        FreezeRotation = 1,
        FreezePositionX = 2,
        FreezePositionY = 4,
        FreezePosition = FreezePositionX | FreezePositionY,
        FreezeAll = FreezeRotation | FreezePosition
    };
    typedef Flags<Rigidbody2DConstraintsBits> Rigidbody2DConstraints;
    CW_FLAGS_OPERATORS(Rigidbody2DConstraintsBits);

    enum class ForceMode
    {
        Force,
        Impulse
    };

    enum class RigidbodyBodyType
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    };

    enum class RigidbodySleepMode
    {
        NeverSleep,
        StartAwake,
        StartSleeping
    };

    struct Rigidbody2DComponent : public ComponentBase
    {
        Rigidbody2DComponent() : ComponentBase() {}
        Rigidbody2DComponent(const Rigidbody2DComponent& rb) = default;

        void SetLayerMask(uint32_t layerMask, Entity e);
        uint32_t GetLayerMask() const { return m_LayerMask; }

        void SetBodyType(RigidbodyBodyType bodyType);
        void SetGravityScale(float scale);
        void SetMass(float mass);
        void SetConstraints(Rigidbody2DConstraints constraints);
        void SetSleepMode(RigidbodySleepMode sleepMode);
        void SetContinuousCollisionDetection(bool value);
        void SetAngularDrag(float value);
        void SetLinearDrag(float value);

        float GetMass() const { return m_Mass; }
        float GetGravityScale() const { return m_GravityScale; }
        Rigidbody2DConstraints GetConstraints() const { return m_Constraints; }
        RigidbodyBodyType GetBodyType() const { return m_Type; }
        RigidbodySleepMode GetSleepMode() const { return m_SleepMode; }
        bool GetContinuousCollisionDetection() const { return m_ContinuousCollisionDetection; }
        float GetAngularDrag() const { return m_AngularDrag; }
        float GetLinearDrag() const { return m_LinearDrag; }

        b2Body* RuntimeBody = nullptr;

    private:
        RigidbodyBodyType m_Type = RigidbodyBodyType::Static;
        RigidbodySleepMode m_SleepMode = RigidbodySleepMode::StartAwake;
        Rigidbody2DConstraints m_Constraints = Rigidbody2DConstraintsBits::None;
        uint32_t m_LayerMask = 0;
        float m_Mass = 1.0f;
        float m_GravityScale = 1.0f;
        float m_LinearDrag = 0.0f;
        float m_AngularDrag = 0.05f;
        bool m_ContinuousCollisionDetection = false;
    };

    template <> void ComponentEditorWidget<Rigidbody2DComponent>(Entity e);

    struct Collider2D : ComponentBase
    {
        Collider2D() : ComponentBase() {}

        glm::vec2 Offset = { 0.0f, 0.0f };
        PhysicsMaterial2D Material;
        bool IsTrigger = false;

        void OnCollisionBegin(const Collision2D& col)
        {
            if (CallbackEnter)
                CallbackEnter(col);
        }

        void OnCollisionEnd(const Collision2D& col)
        {
            if (CallbackExit)
                CallbackExit(col);
        }

        b2Fixture* RuntimeFixture = nullptr;

        std::function<void(const Collision2D&)> CallbackEnter;
        std::function<void(const Collision2D&)> CallbackExit;
    };

    struct BoxCollider2DComponent : public Collider2D
    {
        glm::vec2 Size = { 0.5f, 0.5f };

        BoxCollider2DComponent() : Collider2D() {}
        BoxCollider2DComponent(const BoxCollider2DComponent& collider) = default;
    };

    template <> void ComponentEditorWidget<BoxCollider2DComponent>(Entity e);

    struct CircleCollider2DComponent : public Collider2D
    {
        float Radius = 0.5f;

        CircleCollider2DComponent() : Collider2D() {}
        CircleCollider2DComponent(const CircleCollider2DComponent& collider) = default;
    };

    template <> void ComponentEditorWidget<CircleCollider2DComponent>(Entity e);

    template <typename... Component> struct ComponentGroup
    {
    };
    using AllComponents =
      ComponentGroup<TransformComponent, CameraComponent, TextComponent, SpriteRendererComponent, MeshRendererComponent,
                     AudioSourceComponent, AudioListenerComponent, RelationshipComponent, MonoScriptComponent,
                     Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent>;
} // namespace Crowny
