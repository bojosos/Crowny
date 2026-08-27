#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

#include "Crowny/Common/Color.h"
#include "Crowny/Scene/SceneCamera.h"

#include "Crowny/Animation/AnimationPlayer.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Math.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/Physics/PhysicsCollision.h"
#include "Crowny/Physics/Physics3DTypes.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/RenderLight.h"

#include <glm/gtx/quaternion.hpp>

#include <atomic>
#include <limits>
#include <memory>

namespace Crowny
{
    class MeshUploadResult;

    inline std::atomic<uint64_t> s_NextAvailableId{ 1 };

    template <class Component> void ComponentEditorWidget(Entity entity);
    struct ComponentBase
    {
        ComponentBase() : InstanceId(s_NextAvailableId.fetch_add(1, std::memory_order_relaxed)) {}
        ComponentBase(const ComponentBase&) : ComponentBase() {}
        ComponentBase& operator=(const ComponentBase&) { return *this; }

        uint64_t InstanceId;
    };

    struct IDComponent : public ComponentBase
    {
        UUID Uuid;

        IDComponent() : ComponentBase() {};
        IDComponent(const IDComponent&) = default;
        IDComponent(const UUID& uuid) : Uuid(uuid) {}
    };

    struct TagComponent : public ComponentBase
    {
        String Tag;

        TagComponent() : ComponentBase() {}
        TagComponent(const TagComponent&) = default;
        TagComponent(const String& tag) : Tag(tag) {}

        operator String&() { return Tag; }
        operator const String&() const { return Tag; }
    };

    class TransformComponent : public ComponentBase
    {
    public:
        enum
        {
            DIRTY_LOCAL = 1 << 0,
            DIRTY_WORLD = 1 << 1
        };

    private:
        friend class Scene;
        // Kept at the top since it is the most likely one to be used during runtime.
        mutable glm::mat4 WorldTransformCache = glm::mat4(1.0f);
        mutable glm::mat4 LocalTransformCache = glm::mat4(1.0f);

        mutable Transform WorldTransform;
        mutable Transform LocalTransform;

        mutable uint32_t TransformDirtyFlags = DIRTY_LOCAL | DIRTY_WORLD;

    public:
        TransformComponent() : ComponentBase() {}
        TransformComponent(const TransformComponent& other) : ComponentBase(other), LocalTransform(other.GetLocalTransform()) {}
        TransformComponent& operator=(const TransformComponent& other)
        {
            if (this == &other)
                return *this;
            ComponentBase::operator=(other);
            LocalTransform = other.GetLocalTransform();
            TransformDirtyFlags = DIRTY_LOCAL | DIRTY_WORLD;
            return *this;
        }

        bool IsCachedWorldTransformValid() const { return (TransformDirtyFlags & DIRTY_WORLD) == 0; }
        bool IsCachedLocalTransformValid() const { return (TransformDirtyFlags & DIRTY_LOCAL) == 0; }

        void SetPosition(const glm::vec3& position)
        {
            LocalTransform.SetPosition(position);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetRotation(const glm::quat& rotation)
        {
            LocalTransform.SetRotation(rotation);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetScale(const glm::vec3& scale)
        {
            LocalTransform.SetScale(scale);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetWorldPosition(const glm::vec3& position, const Entity& parent)
        {
            if (parent)
                LocalTransform.SetWorldPosition(position, parent.GetTransform().GetWorldTransform(parent.GetParent()));
            else
                LocalTransform.SetPosition(position);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetWorldRotation(const glm::quat& rotation, const Entity& parent)
        {
            if (parent)
                LocalTransform.SetWorldRotation(rotation, parent.GetTransform().GetWorldTransform(parent.GetParent()));
            else
                LocalTransform.SetRotation(rotation);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetWorldScale(const glm::vec3& scale, const Entity& parent)
        {
            if (parent)
                LocalTransform.SetWorldScale(scale, parent.GetTransform().GetWorldTransform(parent.GetParent()));
            else
                LocalTransform.SetScale(scale);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        void SetWorldTransform(const Transform& worldTransform, const Entity& parent)
        {
            if (parent)
            {
                LocalTransform = worldTransform;
                LocalTransform.MakeLocal(parent.GetTransform().GetWorldTransform(parent.GetParent()));
            }
            else
            {
                LocalTransform = worldTransform;
            }
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
        }

        bool SetWorldMatrix(const glm::mat4& worldMatrix, const Entity& parent)
        {
            glm::mat4 localMatrix = worldMatrix;
            if (parent)
            {
                const glm::mat4& parentWorld = parent.GetWorldMatrix();
                if (std::abs(glm::determinant(glm::mat3(parentWorld))) <= std::numeric_limits<float>::epsilon())
                    return false;
                localMatrix = glm::inverse(parentWorld) * worldMatrix;
            }

            glm::vec3 position;
            glm::quat rotation;
            glm::vec3 scale;
            if (!Math::DecomposeMatrix(localMatrix, position, rotation, scale))
                return false;
            LocalTransform = Transform(position, glm::normalize(rotation), scale);
            TransformDirtyFlags |= DIRTY_LOCAL | DIRTY_WORLD;
            return true;
        }

        void InvalidateWorld() { TransformDirtyFlags |= DIRTY_WORLD; }

        const Transform& GetLocalTransform() const
        {
            if (!IsCachedLocalTransformValid())
                UpdateLocalTransform();

            return LocalTransform;
        }

        const Transform& GetWorldTransform(Entity parent) const
        {
            if (!IsCachedWorldTransformValid())
                UpdateWorldTransform(parent);

            return WorldTransform;
        }

        const glm::mat4& GetLocalMatrix() const
        {
            if (!IsCachedLocalTransformValid())
                UpdateLocalTransform();

            return LocalTransformCache;
        }

        const glm::mat4& GetWorldMatrix(Entity parent) const
        {
            if (!IsCachedWorldTransformValid())
                UpdateWorldTransform(parent);

            return WorldTransformCache;
        }

        void UpdateLocalTransform() const
        {
            LocalTransformCache = LocalTransform.GetMatrix();
            TransformDirtyFlags &= ~DIRTY_LOCAL;
        }

        void UpdateWorldTransform(const Entity& parent) const
        {
            if (parent)
            {
                WorldTransformCache = parent.GetWorldMatrix() * GetLocalMatrix();
                Math::DecomposeMatrix(WorldTransformCache, WorldTransform.m_Position, WorldTransform.m_Rotation, WorldTransform.m_Scale);
            }
            else
            {
                WorldTransformCache = GetLocalMatrix();
                WorldTransform = LocalTransform;
            }
            TransformDirtyFlags &= ~DIRTY_WORLD;
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

    struct LightComponent : public ComponentBase
    {
        LightType Type = LightType::Point;
        glm::vec3 Color = glm::vec3(1.0f);
        // Lux for directional lights and lumens for point and spot lights.
        float Intensity = 1000.0f;
        float Range = 10.0f;
        float SpotInnerAngle = glm::radians(25.0f);
        float SpotOuterAngle = glm::radians(35.0f);
        float SourceRadius = 0.0f;
        bool UseColorTemperature = false;
        float Temperature = 6500.0f;
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        bool Enabled = true;
        bool AffectDiffuse = true;
        bool AffectSpecular = true;
        bool Volumetric = false;
        LightShadowSettings Shadows;

        LightComponent() : ComponentBase() {}
        LightComponent(const LightComponent&) = default;
    };

    template <> void ComponentEditorWidget<LightComponent>(Entity e);

    enum class TextOverflow
    {
        Overflow,
        Ellipses,
        Ellipsis = Ellipses,
        Truncate
    };

    enum class TextWrapMode
    {
        Word,
        Character,
        WordThenCharacter
    };

    enum class TextHorizontalAlignment
    {
        Left,
        Center,
        Right,
        Justified,
        Flush
    };

    enum class TextVerticalAlignment
    {
        Top,
        Middle,
        Bottom,
        Baseline,
        Midline
    };

    enum class TextFontStyleBits
    {
        None = 0,
        Bold = 1 << 0,
        Italic = 1 << 1,
        Underline = 1 << 2,
        Strikethrough = 1 << 3
    };
    typedef Flags<TextFontStyleBits> TextFontStyle;
    CW_FLAGS_OPERATORS(TextFontStyleBits);

    struct TextComponent : public ComponentBase
    {
        String Text;
        AssetHandle<Crowny::Font> Font;
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        float Size = 36.0f;
        bool AutoSize = false;
        float AutoSizeMin = 8.0f;
        float AutoSizeMax = 72.0f;

        // Extends right and down from the entity origin. A zero axis is unbounded.
        glm::vec2 LayoutSize{ 0.0f };
        bool Wrapping = true;
        TextWrapMode WrapMode = TextWrapMode::WordThenCharacter;
        TextOverflow Overflow = TextOverflow::Overflow;
        bool ClipToBounds = false;
        uint32_t MaxLines = 0;

        TextHorizontalAlignment HorizontalAlignment = TextHorizontalAlignment::Left;
        TextVerticalAlignment VerticalAlignment = TextVerticalAlignment::Top;
        TextFontStyle FontStyle = TextFontStyleBits::None;

        glm::vec4 OutlineColor{ 0.0f, 0.0f, 0.0f, 0.0f };
        float Thickness = 0.8f;

        float CharacterSpacing = 0.0f;
        float WordSpacing = 0.0f;
        float LineSpacing = 0.0f;
        float ParagraphSpacing = 0.0f;

        bool UseCustomDecorationColor = false;
        glm::vec4 DecorationColor{ 1.0f };
        // Zero uses the font metric. Offsets are added to the font-derived position.
        float DecorationThickness = 0.0f;
        float UnderlineOffset = 0.0f;
        float StrikethroughOffset = 0.0f;

        bool UseKerning = true;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;

        TextComponent();
        TextComponent(const TextComponent&) = default;
        TextComponent(const String& text) : Text(text) {}
    };

    template <> void ComponentEditorWidget<TextComponent>(Entity e);

    struct SpriteRendererComponent : public ComponentBase
    {
        AssetHandle<Crowny::Texture> Texture;
        glm::vec4 Color{ 1.0f };
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;

        SpriteRendererComponent() : ComponentBase() {}
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
    };

    template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e);

    struct MeshRendererComponent : public ComponentBase
    {
        AssetHandle<Mesh> MeshHandle;
        Vector<AssetHandle<Material>> Materials; // One per sub-mesh; index 0 is default/fallback
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        float LodBias = 0.0f;
        int32_t RenderLayerOrder = 0;
        bool Visible = true;
        bool CastShadows = true;
        bool ReceiveShadows = true;
        bool MotionVectors = true;

        AssetHandle<Material> GetMaterial(uint32_t index = 0) const
        {
            if (index < Materials.size())
                return Materials[index];
            if (!Materials.empty())
                return Materials[0];
            return {};
        }

        void SetMaterial(uint32_t index, const AssetHandle<Material>& material)
        {
            if (index >= Materials.size())
                Materials.resize(index + 1);
            Materials[index] = material;
        }

        uint32_t GetMaterialCount() const { return (uint32_t)Materials.size(); }

        MeshRendererComponent() : ComponentBase() {}
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e);

    struct ProceduralMeshComponent : public ComponentBase
    {
        AssetHandle<NodeGraphAsset> Graph;        // The node graph to evaluate
        Vector<AssetHandle<Material>> Materials;  // Materials for rendering (same as MeshRendererComponent)
        UnorderedMap<UUID, PinValue> InputValues; // Values for graph inputs

        // Internal state managed by the procedural mesh system
        Ref<MeshData> CpuMeshData;                  // Latest evaluation result (CPU side, sim thread)
        Ref<Mesh> GpuMesh;                          // GPU-uploaded mesh (created on render thread)
        AssetHandle<Mesh> RuntimeMeshHandle;        // Stable handle reused by render extraction
        bool NeedsEvaluation = true;                // Graph inputs changed, re-evaluate on sim thread
        bool NeedsGpuUpload = false;                // New CpuMeshData ready, upload on render thread
        uint32_t LastEvaluatedVersion = 0xFFFFFFFF; // Version of the graph when last evaluated

        // The queued command shares this result, so it remains valid if the component is destroyed.
        std::shared_ptr<MeshUploadResult> PendingGpuResult;
        bool GpuUploadPending = false;

        ProceduralMeshComponent() : ComponentBase() {}
        ProceduralMeshComponent(const ProceduralMeshComponent& other) : ComponentBase(other) { CopySettings(other); }
        ProceduralMeshComponent& operator=(const ProceduralMeshComponent& other)
        {
            if (this != &other)
            {
                ComponentBase::operator=(other);
                CopySettings(other);
            }
            return *this;
        }

    private:
        void CopySettings(const ProceduralMeshComponent& other)
        {
            Graph = other.Graph;
            Materials = other.Materials;
            InputValues = other.InputValues;
            CpuMeshData = nullptr;
            GpuMesh = nullptr;
            RuntimeMeshHandle = nullptr;
            NeedsEvaluation = true;
            NeedsGpuUpload = false;
            LastEvaluatedVersion = 0xFFFFFFFF;
            PendingGpuResult = nullptr;
            GpuUploadPending = false;
        }
    };

    template <> void ComponentEditorWidget<ProceduralMeshComponent>(Entity e);

    struct RelationshipComponent : public ComponentBase
    {
        Vector<Entity> Children;
        Entity Parent;
        uint32_t SiblingIndex = 0;

        RelationshipComponent() : ComponentBase() {}
        RelationshipComponent(const RelationshipComponent& other);
        RelationshipComponent(RelationshipComponent&& other) noexcept;
        RelationshipComponent(const Entity& parent) : Parent(parent) {}

        RelationshipComponent& operator=(const RelationshipComponent& other);
        RelationshipComponent& operator=(RelationshipComponent&& other) noexcept;
    };

    class AudioListenerComponent : public ComponentBase
    {
    public:
        static constexpr bool in_place_delete = true;

        AudioListenerComponent() = default;
        AudioListenerComponent(const AudioListenerComponent& other) : ComponentBase(other) {}
        AudioListenerComponent& operator=(const AudioListenerComponent& other)
        {
            if (this != &other)
                ComponentBase::operator=(other);
            return *this;
        }

        void Initialize();
        void OnTransformChanged(const Transform& transform);

    private:
        Ref<AudioListener> m_Internal;
    };

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e);

    struct AudioSourceComponent : public ComponentBase
    {
    public:
        static constexpr bool in_place_delete = true;

        AudioSourceComponent() : ComponentBase() {}
        AudioSourceComponent(const AudioSourceComponent& other);
        AudioSourceComponent& operator=(const AudioSourceComponent& other);

        void OnInitialize();
        void ApplyRuntimeSettings();
        void OnEnabled();
        void OnDisabled();
        void OnDestroyed();
        void OnTransformChanged(const Transform& transform);

        void SetVolume(float volume);
        void SetPitch(float pitch);
        void SetClip(const AssetHandle<AudioClip>& clip);
        void SetPlayOnAwake(bool playOnAwake);
        void SetMinDistance(float minDistnace);
        void SetMaxDistance(float maxDistance);
        void SetLooping(bool loop);
        void SetIsMuted(bool muted);
        void SetTime(float time);
        void SetBusName(const String& busName);
        void SetLowPassGain(float gainHF);
        void SetHighPassGain(float gainLF);
        void SetConeInnerAngle(float degrees);
        void SetConeOuterAngle(float degrees);
        void SetConeOuterGain(float gain);
        void SetConeOuterGainHF(float gainHF);

        float GetVolume() const { return m_Volume; }
        float GetPitch() const { return m_Pitch; }
        AssetHandle<AudioClip> GetClip() const { return m_AudioClip; }
        bool GetPlayOnAwake() const { return m_PlayOnAwake; }
        float GetMinDistance() const { return m_MinDistance; }
        float GetMaxDistance() const { return m_MaxDistance; }
        bool GetLooping() const { return m_Loop; }
        bool GetIsMuted() const { return m_IsMuted; }
        float GetTime() const { return m_Time; }
        const String& GetBusName() const { return m_BusName; }
        float GetLowPassGain() const { return m_LowPassGain; }
        float GetHighPassGain() const { return m_HighPassGain; }
        float GetConeInnerAngle() const { return m_ConeInnerAngle; }
        float GetConeOuterAngle() const { return m_ConeOuterAngle; }
        float GetConeOuterGain() const { return m_ConeOuterGain; }
        float GetConeOuterGainHF() const { return m_ConeOuterGainHF; }

        void Play();
        void Pause();
        void Stop();

        AudioSourceState GetState() const;
        const AudioSource* GetRuntimeSource() const noexcept { return m_Internal.Get(); }

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
        String m_BusName; // empty = master bus of active mixer (or no bus when no active mixer)
        float m_LowPassGain = 1.0f;
        float m_HighPassGain = 1.0f;
        float m_ConeInnerAngle = 360.0f;
        float m_ConeOuterAngle = 360.0f;
        float m_ConeOuterGain = 0.0f;
        float m_ConeOuterGainHF = 1.0f;

        Ref<AudioSource> m_Internal;
    };

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e);

    struct PersistedScriptState
    {
        ScriptTypeIdentity Identity;
        Ref<SerializableObject> Fields;
        ScriptState ManagedState;
    };

    class MonoScript
    {
    public:
        using LifecycleThunk = void(CW_THUNKCALL*)(MonoObject*, MonoException**);

        struct RuntimeCallback
        {
            MonoObject* Instance = nullptr;
            LifecycleThunk Thunk = nullptr;

            void Invoke() const;
            explicit operator bool() const { return Instance != nullptr && Thunk != nullptr; }
        };

        MonoScript();
        explicit MonoScript(ScriptTypeIdentity identity);
        MonoScript(MonoReflectionType* runtimeType);
        MonoScript(const String& assemblyName, MonoReflectionType* runtimeType);
        MonoScript(const MonoScript& other);
        MonoScript& operator=(const MonoScript& other);
        MonoScript(MonoScript&& other) noexcept = default;
        MonoScript& operator=(MonoScript&& other) noexcept = default;

        void SetClassName(const String& className);
        MonoClass* GetManagedClass() const;
        MonoReflectionType* GetRuntimeType() const { return m_RuntimeType; }
        MonoObject* GetManagedInstance() const;

        Ref<SerializableObjectInfo> GetObjectInfo() const { return m_ObjectInfo; }

        PersistedScriptState CapturePersistedState() const;
        bool ApplyPersistedState(const PersistedScriptState& state);

        const ScriptTypeIdentity& GetTypeIdentity() const { return m_Identity; }
        const String& GetAssemblyName() const { return m_Identity.Assembly; }
        const String& GetTypeName() const { return m_Identity.TypeName; }
        const String& GetNamespace() const { return m_Identity.Namespace; }
        ScriptInstanceHandle GetRuntimeHandle() const { return m_RuntimeHandle; }
        void SetRuntimeHandle(ScriptInstanceHandle handle) { m_RuntimeHandle = handle; }
        void ClearRuntimeHandle() { m_RuntimeHandle = {}; }
        const ScriptState& GetManagedState() const { return m_ManagedState; }
        void SetManagedState(ScriptState state) { m_ManagedState = std::move(state); }

        void OnInitialize(ScriptEntityBehaviour* entityBehaviour);
        void Create(Entity entity);
        void ClearRuntimeInstance();
        void OnStart();
        void OnUpdate();
        void OnDestroy();
        RuntimeCallback GetStartCallback() const;
        RuntimeCallback GetUpdateCallback() const;
        RuntimeCallback GetDestroyCallback() const;

        void OnCollisionEnter2D(const Collision2D& collision);
        void OnCollisionStay2D(const Collision2D& collision);
        void OnCollisionExit2D(const Collision2D& collision);

        void OnTriggerEnter2D(Entity other);
        void OnTriggerStay2D(Entity other);
        void OnTriggerExit2D(Entity other);

        void OnCollisionEnter3D(const Collision3D& collision);
        void OnCollisionStay3D(const Collision3D& collision);
        void OnCollisionExit3D(const Collision3D& collision);

        void OnTriggerEnter3D(Entity other);
        void OnTriggerStay3D(Entity other);
        void OnTriggerExit3D(Entity other);

        uint64_t InstanceId; // These also require one for scripting

    private:
        bool ResolveObjectInfo();
        void ResetRuntimeCallbacks();

        typedef void(CW_THUNKCALL* OnCollisionEnterThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
        typedef void(CW_THUNKCALL* OnCollisionStayThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
        typedef void(CW_THUNKCALL* OnCollisionExitThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
        typedef void(CW_THUNKCALL* OnTriggerEnterThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
        typedef void(CW_THUNKCALL* OnTriggerStayThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);
        typedef void(CW_THUNKCALL* OnTriggerExitThunkDef)(MonoObject* object, MonoObject* data, MonoException** ex);

        LifecycleThunk m_OnStartThunk = nullptr;
        LifecycleThunk m_OnUpdateThunk = nullptr;
        LifecycleThunk m_OnDestroyThunk = nullptr;

        OnCollisionEnterThunkDef m_OnCollisionEnterThunk = nullptr;
        OnCollisionStayThunkDef m_OnCollisionStayThunk = nullptr;
        OnCollisionExitThunkDef m_OnCollisionExitThunk = nullptr;
        OnTriggerEnterThunkDef m_OnTriggerEnterThunk = nullptr;
        OnTriggerStayThunkDef m_OnTriggerStayThunk = nullptr;
        OnTriggerExitThunkDef m_OnTriggerExitThunk = nullptr;
        OnCollisionEnterThunkDef m_OnCollisionEnter3DThunk = nullptr;
        OnCollisionStayThunkDef m_OnCollisionStay3DThunk = nullptr;
        OnCollisionExitThunkDef m_OnCollisionExit3DThunk = nullptr;
        OnTriggerEnterThunkDef m_OnTriggerEnter3DThunk = nullptr;
        OnTriggerStayThunkDef m_OnTriggerStay3DThunk = nullptr;
        OnTriggerExitThunkDef m_OnTriggerExit3DThunk = nullptr;

        ScriptTypeIdentity m_Identity;
        bool m_MissingType = false;

        Ref<SerializableObject> m_SerializedObjectData;
        Ref<SerializableObjectInfo> m_ObjectInfo;
        MonoReflectionType* m_RuntimeType = nullptr;
        MonoClass* m_Class = nullptr;
        ScriptEntityBehaviour* m_ScriptEntityBehaviour = nullptr;
        ScriptInstanceHandle m_RuntimeHandle;
        ScriptState m_ManagedState;
    };

    class MonoScriptComponent : public ComponentBase
    {
    public:
        MonoScriptComponent() : ComponentBase() {}
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
        Force = 0,
        Impulse = 1
    };

    enum class RigidbodyBodyType
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    };

    enum class RigidbodySleepMode
    {
        NeverSleep = 0,
        StartAwake = 1,
        StartSleeping = 2
    };

    enum class CollisionDetectionMode2D
    {
        Discrete = 0,
        Continuous = 1
    };

    enum class RigidbodyInterpolation
    {
        None = 0,
        Interpolate = 1,
        Extrapolate = 2
    };

    struct Rigidbody2DComponent : public ComponentBase
    {
        static constexpr bool in_place_delete = true;

        Rigidbody2DComponent() : ComponentBase() {}
        Rigidbody2DComponent(const Rigidbody2DComponent& other) : ComponentBase(other) { CopySettings(other); }
        Rigidbody2DComponent& operator=(const Rigidbody2DComponent& other)
        {
            if (this != &other)
            {
                ComponentBase::operator=(other);
                CopySettings(other);
            }
            return *this;
        }

        void SetLayerMask(uint32_t layerMask, Entity e);
        uint32_t GetLayerMask() const { return m_LayerMask; }

        void SetBodyType(RigidbodyBodyType bodyType);
        void SetGravityScale(float scale);
        void SetMass(float mass);
        void SetConstraints(Rigidbody2DConstraints constraints);
        void SetSleepMode(RigidbodySleepMode sleepMode);
        void SetCollisionDetectionMode(CollisionDetectionMode2D value);
        void SetAngularDrag(float value);
        void SetLinearDrag(float value);
        void SetAutoMass(bool autoMass, Entity entity);
        void SetCenterOfMass(const glm::vec2& center);
        void SetInterpolationMode(RigidbodyInterpolation interpolation);
        void SetInertia(float inertia);

        float GetMass() const;
        float GetConfiguredMass() const { return m_Mass; }
        float GetGravityScale() const { return m_GravityScale; }
        Rigidbody2DConstraints GetConstraints() const { return m_Constraints; }
        RigidbodyBodyType GetBodyType() const { return m_Type; }
        RigidbodySleepMode GetSleepMode() const { return m_SleepMode; }
        CollisionDetectionMode2D GetCollisionDetectionMode() const { return m_ContinuousCollisionDetection; }
        float GetAngularDrag() const { return m_AngularDrag; }
        float GetLinearDrag() const { return m_LinearDrag; }
        bool GetAutoMass() const { return m_AutoMass; }
        glm::vec2 GetCenterOfMass() const;
        const glm::vec2& GetConfiguredCenterOfMass() const { return m_CenterOfMass; }
        RigidbodyInterpolation GetInterpolationMode() const { return m_InterpolationMode; }
        float GetInertia() const;
        float GetConfiguredInertia() const { return m_Inertia; }

        void* RuntimeBody = nullptr;
        glm::vec2 RuntimePreviousPosition{ 0.0f };
        float RuntimePreviousRotation = 0.0f;
        bool RuntimeHasPreviousState = false;

    private:
        void CopySettings(const Rigidbody2DComponent& other)
        {
            m_Type = other.m_Type;
            m_SleepMode = other.m_SleepMode;
            m_Constraints = other.m_Constraints;
            m_LayerMask = other.m_LayerMask;
            m_Mass = other.m_Mass;
            m_GravityScale = other.m_GravityScale;
            m_LinearDrag = other.m_LinearDrag;
            m_AngularDrag = other.m_AngularDrag;
            m_AutoMass = other.m_AutoMass;
            m_Inertia = other.m_Inertia;
            m_CenterOfMass = other.m_CenterOfMass;
            m_ContinuousCollisionDetection = other.m_ContinuousCollisionDetection;
            m_InterpolationMode = other.m_InterpolationMode;
        }

        RigidbodyBodyType m_Type = RigidbodyBodyType::Static;
        RigidbodySleepMode m_SleepMode = RigidbodySleepMode::StartAwake;
        Rigidbody2DConstraints m_Constraints = Rigidbody2DConstraintsBits::None;
        uint32_t m_LayerMask = 0;
        float m_Mass = 1.0f;
        float m_GravityScale = 1.0f;
        float m_LinearDrag = 0.0f;
        float m_AngularDrag = 0.05f;
        bool m_AutoMass = false;
        float m_Inertia = 1.0f;
        glm::vec2 m_CenterOfMass = { 0.0f, 0.0f };
        CollisionDetectionMode2D m_ContinuousCollisionDetection = CollisionDetectionMode2D::Discrete;
        RigidbodyInterpolation m_InterpolationMode = RigidbodyInterpolation::None;
    };

    template <> void ComponentEditorWidget<Rigidbody2DComponent>(Entity e);

    struct Collider2D : ComponentBase
    {
        static constexpr bool in_place_delete = true;

        Collider2D() : ComponentBase() {}
        Collider2D(const Collider2D& other)
          : ComponentBase(other), m_Offset(other.m_Offset), m_Material(other.m_Material), m_IsTrigger(other.m_IsTrigger)
        {
        }
        Collider2D& operator=(const Collider2D& other)
        {
            if (this != &other)
            {
                ComponentBase::operator=(other);
                m_Offset = other.m_Offset;
                m_Material = other.m_Material;
                m_IsTrigger = other.m_IsTrigger;
            }
            return *this;
        }

        const glm::vec2& GetOffset() const { return m_Offset; }
        bool IsTrigger() const { return m_IsTrigger; }
        const AssetHandle<PhysicsMaterial2D>& GetMaterial() const { return m_Material; }
        const PhysicsMaterialData& GetMaterialData() const;

        void SetIsTrigger(bool trigger);
        void SetMaterial(const AssetHandle<PhysicsMaterial2D>& material);
        void RefreshMaterial();

        void* RuntimeFixture = nullptr;

        glm::vec2 m_Offset = { 0.0f, 0.0f };
        AssetHandle<PhysicsMaterial2D> m_Material;
        bool m_IsTrigger = false;
    };

    struct BoxCollider2DComponent : public Collider2D
    {
        static constexpr bool in_place_delete = true;

        BoxCollider2DComponent();
        BoxCollider2DComponent(const BoxCollider2DComponent& collider) = default;

        const glm::vec2& GetSize() const { return m_Size; }
        void SetSize(const glm::vec2& size, Entity entity);

        void SetOffset(const glm::vec2& size, Entity entity);

    private:
        glm::vec2 m_Size = { 0.5f, 0.5f };
    };

    template <> void ComponentEditorWidget<BoxCollider2DComponent>(Entity e);

    struct CircleCollider2DComponent : public Collider2D
    {
        static constexpr bool in_place_delete = true;

        CircleCollider2DComponent();
        CircleCollider2DComponent(const CircleCollider2DComponent& collider) = default;

        float GetRadius() const { return m_Radius; }
        void SetRadius(float radius, Entity entity);

        void SetOffset(const glm::vec2& size, Entity entity);

    private:
        float m_Radius = 0.5f;
    };

    template <> void ComponentEditorWidget<CircleCollider2DComponent>(Entity e);

    struct Rigidbody3DComponent : public ComponentBase
    {
        static constexpr bool in_place_delete = true;

        Rigidbody3DComponent() : ComponentBase() {}
        Rigidbody3DComponent(const Rigidbody3DComponent& other);
        Rigidbody3DComponent& operator=(const Rigidbody3DComponent& other);

        PhysicsBodyType3D GetBodyType() const { return m_Type; }
        float GetMass() const { return m_Mass; }
        bool GetAutoMass() const { return m_AutoMass; }
        float GetGravityScale() const { return m_GravityScale; }
        float GetLinearDamping() const { return m_LinearDamping; }
        float GetAngularDamping() const { return m_AngularDamping; }
        const glm::vec3& GetCenterOfMass() const { return m_CenterOfMass; }
        bool GetAllowSleep() const { return m_AllowSleep; }
        bool GetStartAwake() const { return m_StartAwake; }
        bool GetContinuousCollision() const { return m_ContinuousCollision; }
        bool GetLockRotationX() const { return m_LockRotationX; }
        bool GetLockRotationY() const { return m_LockRotationY; }
        bool GetLockRotationZ() const { return m_LockRotationZ; }
        const PhysicsFilter3D& GetFilter() const { return m_Filter; }

        void SetBodyType(PhysicsBodyType3D type, Entity entity);
        void SetMass(float mass, Entity entity);
        void SetAutoMass(bool autoMass, Entity entity);
        void SetGravityScale(float scale);
        void SetDamping(float linear, float angular);
        void SetCenterOfMass(const glm::vec3& center, Entity entity);
        void SetAllowSleep(bool allowSleep, Entity entity);
        void SetStartAwake(bool startAwake, Entity entity);
        void SetContinuousCollision(bool continuous, Entity entity);
        void SetRotationLocks(bool x, bool y, bool z, Entity entity);
        void SetFilter(const PhysicsFilter3D& filter);

        glm::vec3 GetLinearVelocity() const;
        glm::vec3 GetAngularVelocity() const;
        void SetLinearVelocity(const glm::vec3& velocity);
        void SetAngularVelocity(const glm::vec3& velocity);
        void AddForce(const glm::vec3& force, PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void AddForceAt(const glm::vec3& force, const glm::vec3& point, PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void AddTorque(const glm::vec3& torque, PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void SetAwake(bool awake);
        bool IsAwake() const;

        PhysicsBody3DHandle RuntimeBody;

    private:
        void CopySettings(const Rigidbody3DComponent& other);

        PhysicsBodyType3D m_Type = PhysicsBodyType3D::Static;
        float m_Mass = 1.0f;
        bool m_AutoMass = true;
        float m_GravityScale = 1.0f;
        float m_LinearDamping = 0.0f;
        float m_AngularDamping = 0.05f;
        glm::vec3 m_CenterOfMass{ 0.0f };
        bool m_AllowSleep = true;
        bool m_StartAwake = true;
        bool m_ContinuousCollision = false;
        bool m_LockRotationX = false;
        bool m_LockRotationY = false;
        bool m_LockRotationZ = false;
        PhysicsFilter3D m_Filter;
        glm::vec3 m_LinearVelocity{ 0.0f };
        glm::vec3 m_AngularVelocity{ 0.0f };
    };

    template <> void ComponentEditorWidget<Rigidbody3DComponent>(Entity e);

    struct Collider3D : public ComponentBase
    {
        static constexpr bool in_place_delete = true;

        Collider3D();
        Collider3D(const Collider3D& other);
        Collider3D& operator=(const Collider3D& other);

        const glm::vec3& GetOffset() const { return m_Offset; }
        const glm::quat& GetRotation() const { return m_Rotation; }
        bool IsTrigger() const { return m_IsTrigger; }
        const AssetHandle<PhysicsMaterial3D>& GetMaterial() const { return m_Material; }
        const PhysicsMaterialData& GetMaterialData() const;
        const PhysicsFilter3D& GetFilter() const { return m_Filter; }

        void SetOffset(const glm::vec3& offset, Entity entity);
        void SetRotation(const glm::quat& rotation, Entity entity);
        void SetIsTrigger(bool trigger);
        void SetMaterial(const AssetHandle<PhysicsMaterial3D>& material);
        void RefreshMaterial();
        void SetFilter(const PhysicsFilter3D& filter, Entity entity);

        PhysicsShape3DHandle RuntimeShape;

    protected:
        void CopySettings(const Collider3D& other);

        glm::vec3 m_Offset{ 0.0f };
        glm::quat m_Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        bool m_IsTrigger = false;
        AssetHandle<PhysicsMaterial3D> m_Material;
        PhysicsFilter3D m_Filter;
    };

    struct BoxCollider3DComponent : public Collider3D
    {
        static constexpr bool in_place_delete = true;

        BoxCollider3DComponent() = default;
        BoxCollider3DComponent(const BoxCollider3DComponent& other);
        BoxCollider3DComponent& operator=(const BoxCollider3DComponent& other);

        const glm::vec3& GetSize() const { return m_Size; }
        void SetSize(const glm::vec3& size, Entity entity);

    private:
        glm::vec3 m_Size{ 1.0f };
    };

    template <> void ComponentEditorWidget<BoxCollider3DComponent>(Entity e);

    struct SphereCollider3DComponent : public Collider3D
    {
        static constexpr bool in_place_delete = true;

        SphereCollider3DComponent() = default;
        SphereCollider3DComponent(const SphereCollider3DComponent& other);
        SphereCollider3DComponent& operator=(const SphereCollider3DComponent& other);

        float GetRadius() const { return m_Radius; }
        void SetRadius(float radius, Entity entity);

    private:
        float m_Radius = 0.5f;
    };

    template <> void ComponentEditorWidget<SphereCollider3DComponent>(Entity e);

    struct CapsuleCollider3DComponent : public Collider3D
    {
        static constexpr bool in_place_delete = true;

        CapsuleCollider3DComponent() = default;
        CapsuleCollider3DComponent(const CapsuleCollider3DComponent& other);
        CapsuleCollider3DComponent& operator=(const CapsuleCollider3DComponent& other);

        float GetRadius() const { return m_Radius; }
        float GetHeight() const { return m_Height; }
        void SetRadius(float radius, Entity entity);
        void SetHeight(float height, Entity entity);

    private:
        float m_Radius = 0.5f;
        float m_Height = 2.0f;
    };

    template <> void ComponentEditorWidget<CapsuleCollider3DComponent>(Entity e);

    static_assert(entt::component_traits<Rigidbody2DComponent>::in_place_delete);
    static_assert(entt::component_traits<BoxCollider2DComponent>::in_place_delete);
    static_assert(entt::component_traits<CircleCollider2DComponent>::in_place_delete);
    static_assert(entt::component_traits<Rigidbody3DComponent>::in_place_delete);
    static_assert(entt::component_traits<BoxCollider3DComponent>::in_place_delete);
    static_assert(entt::component_traits<SphereCollider3DComponent>::in_place_delete);
    static_assert(entt::component_traits<CapsuleCollider3DComponent>::in_place_delete);
    static_assert(entt::component_traits<AudioSourceComponent>::in_place_delete);
    static_assert(entt::component_traits<AudioListenerComponent>::in_place_delete);

    struct PrefabComponent : public ComponentBase
    {
        UUID PrefabAssetUuid;
        UUID PrefabEntityUuid;
        // Keep serialized override paths as text, but allow allocation-free lookups
        // from string views and compile-time hashed literals.
        UnorderedSet<String, StringHash, StringEqual> Overrides;

        PrefabComponent() : ComponentBase() {}
        PrefabComponent(const PrefabComponent&) = default;
        PrefabComponent(const UUID& prefabAssetUuid, const UUID& prefabEntityUuid)
          : ComponentBase(), PrefabAssetUuid(prefabAssetUuid), PrefabEntityUuid(prefabEntityUuid)
        {
        }

        bool IsPropertyOverridden(StringView path) const { return Overrides.find(path) != Overrides.end(); }
        bool IsPropertyOverridden(HashedString path) const { return Overrides.find(path) != Overrides.end(); }

        bool IsPropertyOverridden(StringView componentName, StringView propertyName) const
        {
            const size_t expectedSize = componentName.size() + 1 + propertyName.size();
            for (const String& path : Overrides)
            {
                if (path.size() != expectedSize || path.compare(0, componentName.size(), componentName) != 0 || path[componentName.size()] != '.' ||
                    path.compare(componentName.size() + 1, propertyName.size(), propertyName) != 0)
                    continue;

                return true;
            }
            return false;
        }

        void MarkOverridden(String path) { Overrides.insert(std::move(path)); }
        void ClearOverride(StringView path)
        {
            const auto iter = Overrides.find(path);
            if (iter != Overrides.end())
                Overrides.erase(iter);
        }
    };

    struct AnimationComponent : public ComponentBase
    {
        AssetHandle<AnimationClip> Clip;
        float Speed = 1.0f;
        AnimationWrapMode WrapMode = AnimationWrapMode::Loop;
        bool PlayOnAwake = true;
        bool ApplyRootMotion = false;

        Ref<AnimationPlayer> Player;
        Ref<MeshDeformer> Deformer;
        Ref<Mesh> RuntimeMesh;
        AssetHandle<Mesh> RuntimeMeshHandle;
        std::shared_ptr<MeshUploadResult> PendingGpuResult;
        bool GpuUploadPending = false;
        UUID RuntimeSourceMesh = UUID::EMPTY;
        UUID RuntimeClip = UUID::EMPTY;

        AnimationComponent() = default;
        AnimationComponent(const AnimationComponent& other)
          : ComponentBase(other), Clip(other.Clip), Speed(other.Speed), WrapMode(other.WrapMode), PlayOnAwake(other.PlayOnAwake),
            ApplyRootMotion(other.ApplyRootMotion)
        {
        }
        AnimationComponent& operator=(const AnimationComponent& other)
        {
            if (this != &other)
            {
                ResetRuntime();
                ComponentBase::operator=(other);
                Clip = other.Clip;
                Speed = other.Speed;
                WrapMode = other.WrapMode;
                PlayOnAwake = other.PlayOnAwake;
                ApplyRootMotion = other.ApplyRootMotion;
                m_PlaybackTime = 0.0f;
                m_PlaybackState = AnimationPlaybackState::Stopped;
                m_HasPlaybackState = false;
            }
            return *this;
        }

        const AssetHandle<AnimationClip>& GetClip() const { return Clip; }
        void SetClip(const AssetHandle<AnimationClip>& clip);
        float GetSpeed() const { return Speed; }
        void SetSpeed(float speed);
        AnimationWrapMode GetWrapMode() const { return WrapMode; }
        void SetWrapMode(AnimationWrapMode wrapMode);
        bool GetPlayOnAwake() const { return PlayOnAwake; }
        void SetPlayOnAwake(bool playOnAwake) { PlayOnAwake = playOnAwake; }
        bool GetApplyRootMotion() const { return ApplyRootMotion; }
        void SetApplyRootMotion(bool applyRootMotion) { ApplyRootMotion = applyRootMotion; }

        void Play();
        void Pause();
        void Stop();
        void SetTime(float time);
        float GetTime() const;
        void SetNormalizedTime(float normalizedTime);
        float GetNormalizedTime() const;
        AnimationPlaybackState GetState() const;

        /** Applies the requested playback state after the renderer creates a player. */
        void InitializeRuntimePlayback();
        /** Captures live player state before a renderer-owned runtime object is discarded. */
        void SynchronizeRuntimePlayback();
        void ResetRuntime(bool preservePlayback = false);

    private:
        float m_PlaybackTime = 0.0f;
        AnimationPlaybackState m_PlaybackState = AnimationPlaybackState::Stopped;
        bool m_HasPlaybackState = false;
    };

    template <> void ComponentEditorWidget<AnimationComponent>(Entity e);

    using AllComponents =
      ComponentGroup<TransformComponent, CameraComponent, LightComponent, TextComponent, SpriteRendererComponent, MeshRendererComponent,
                     ProceduralMeshComponent, AudioSourceComponent, AudioListenerComponent, RelationshipComponent, MonoScriptComponent,
                     Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, Rigidbody3DComponent, BoxCollider3DComponent,
                     SphereCollider3DComponent, CapsuleCollider3DComponent, AnimationComponent, PrefabComponent>;

    using TransformChangedNotifyComponents = ComponentGroup<AudioListenerComponent, AudioSourceComponent>;
} // namespace Crowny
