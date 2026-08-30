#include "cwpch.h"

#include "Crowny/Ecs/Components.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/MeshFactory.h"

namespace Crowny
{
    namespace
    {
        ScriptState MakeEmptyScriptState(const ScriptTypeIdentity& identity)
        {
            ScriptState state;
            state.Identity = identity;
            state.Root = ScriptValue::Object({}, identity);
            return state;
        }

        AnimationWrapMode SanitizeWrapMode(AnimationWrapMode wrapMode)
        {
            switch (wrapMode)
            {
            case AnimationWrapMode::Clamp:
            case AnimationWrapMode::Loop:
            case AnimationWrapMode::PingPong:
                return wrapMode;
            default:
                return AnimationWrapMode::Loop;
            }
        }
    } // namespace

    RelationshipComponent::RelationshipComponent(const RelationshipComponent& other)
      : ComponentBase(other), Children(other.Children), Parent(other.Parent), SiblingIndex(other.SiblingIndex)
    {
    }

    RelationshipComponent::RelationshipComponent(RelationshipComponent&& other) noexcept
      : Children(std::move(other.Children)), Parent(other.Parent), SiblingIndex(other.SiblingIndex)
    {
        InstanceId = other.InstanceId;
        other.Parent = {};
        other.SiblingIndex = 0;
    }

    RelationshipComponent& RelationshipComponent::operator=(const RelationshipComponent& other)
    {
        if (this == &other)
            return *this;
        ComponentBase::operator=(other);
        Children = other.Children;
        Parent = other.Parent;
        SiblingIndex = other.SiblingIndex;
        return *this;
    }

    RelationshipComponent& RelationshipComponent::operator=(RelationshipComponent&& other) noexcept
    {
        if (this == &other)
            return *this;
        InstanceId = other.InstanceId;
        Children = std::move(other.Children);
        Parent = other.Parent;
        SiblingIndex = other.SiblingIndex;
        other.Parent = {};
        other.SiblingIndex = 0;
        return *this;
    }

    void AnimationComponent::SetClip(const AssetHandle<AnimationClip>& clip)
    {
        if (Clip.GetUUID() == clip.GetUUID())
            return;

        ResetRuntime(true);
        Clip = clip;
        m_PlaybackTime = 0.0f;
    }

    void AnimationComponent::SetSpeed(float speed)
    {
        Speed = std::isfinite(speed) ? speed : 1.0f;
        if (Player)
            Player->SetSpeed(Speed);
    }

    void AnimationComponent::SetWrapMode(AnimationWrapMode wrapMode)
    {
        WrapMode = SanitizeWrapMode(wrapMode);
        if (Player)
            Player->SetWrapMode(WrapMode);
    }

    void AnimationComponent::Play()
    {
        m_HasPlaybackState = true;
        m_PlaybackState = AnimationPlaybackState::Playing;
        m_PlaybackTime = 0.0f;
        if (Player)
            Player->Play(Clip.GetInternalPtr());
    }

    void AnimationComponent::Pause()
    {
        if (GetState() != AnimationPlaybackState::Playing)
            return;

        m_HasPlaybackState = true;
        m_PlaybackState = AnimationPlaybackState::Paused;
        if (Player)
            Player->Pause();
    }

    void AnimationComponent::Stop()
    {
        m_HasPlaybackState = true;
        m_PlaybackState = AnimationPlaybackState::Stopped;
        m_PlaybackTime = 0.0f;
        if (Player)
            Player->Stop();
    }

    void AnimationComponent::SetTime(float time)
    {
        m_PlaybackTime = std::isfinite(time) ? time : 0.0f;
        if (Player)
            Player->Seek(m_PlaybackTime);
    }

    float AnimationComponent::GetTime() const
    {
        return Player ? Player->GetTime() : m_PlaybackTime;
    }

    void AnimationComponent::SetNormalizedTime(float normalizedTime)
    {
        const float length = Clip ? Clip->GetLength() : 0.0f;
        SetTime(length > 0.0f && std::isfinite(normalizedTime) ? normalizedTime * length : 0.0f);
    }

    float AnimationComponent::GetNormalizedTime() const
    {
        const float length = Clip ? Clip->GetLength() : 0.0f;
        return length > 0.0f ? GetTime() / length : 0.0f;
    }

    AnimationPlaybackState AnimationComponent::GetState() const
    {
        if (Player)
            return Player->GetState();
        if (m_HasPlaybackState)
            return m_PlaybackState;
        return PlayOnAwake ? AnimationPlaybackState::Playing : AnimationPlaybackState::Paused;
    }

    void AnimationComponent::InitializeRuntimePlayback()
    {
        if (!Player || !Clip)
            return;

        const AnimationPlaybackState requestedState =
          m_HasPlaybackState ? m_PlaybackState : (PlayOnAwake ? AnimationPlaybackState::Playing : AnimationPlaybackState::Paused);
        const float requestedTime = m_PlaybackTime;
        Player->SetSpeed(Speed);
        Player->SetWrapMode(SanitizeWrapMode(WrapMode));
        Player->Play(Clip.GetInternalPtr());
        Player->Seek(requestedTime);
        if (requestedState == AnimationPlaybackState::Paused)
            Player->Pause();
        else if (requestedState == AnimationPlaybackState::Stopped)
        {
            Player->Stop();
            Player->Seek(requestedTime);
        }
        SynchronizeRuntimePlayback();
    }

    void AnimationComponent::SynchronizeRuntimePlayback()
    {
        if (!Player)
            return;
        m_PlaybackTime = Player->GetTime();
        m_PlaybackState = Player->GetState();
        m_HasPlaybackState = true;
    }

    void AnimationComponent::ResetRuntime(bool preservePlayback)
    {
        if (preservePlayback)
            SynchronizeRuntimePlayback();
        else
        {
            m_PlaybackTime = 0.0f;
            m_PlaybackState = AnimationPlaybackState::Stopped;
            m_HasPlaybackState = false;
        }
        Player = nullptr;
        Deformer = nullptr;
        RuntimeMesh = nullptr;
        RuntimeMeshHandle = nullptr;
        PendingGpuResult = nullptr;
        GpuUploadPending = false;
        RuntimeSourceMesh = UUID::EMPTY;
        RuntimeClip = UUID::EMPTY;
    }

    void AudioListenerComponent::Initialize()
    {
        if (AudioManager* audioManager = AudioManager::TryGet())
            m_Internal = audioManager->CreateListener();
    }

    void AudioListenerComponent::OnTransformChanged(const Transform& transform)
    {
        if (m_Internal != nullptr)
            m_Internal->OnTransformChanged(transform);
    }

    AudioSourceComponent::AudioSourceComponent(const AudioSourceComponent& other) : ComponentBase(other) { *this = other; }

    AudioSourceComponent& AudioSourceComponent::operator=(const AudioSourceComponent& other)
    {
        if (this == &other)
            return *this;

        ComponentBase::operator=(other);
        m_AudioClip = other.m_AudioClip;
        m_IsMuted = other.m_IsMuted;
        m_Volume = other.m_Volume;
        m_Pitch = other.m_Pitch;
        m_Loop = other.m_Loop;
        m_MinDistance = other.m_MinDistance;
        m_MaxDistance = other.m_MaxDistance;
        m_PlayOnAwake = other.m_PlayOnAwake;
        m_Time = other.m_Time;
        m_BusName = other.m_BusName;
        m_LowPassGain = other.m_LowPassGain;
        m_HighPassGain = other.m_HighPassGain;
        m_ConeInnerAngle = other.m_ConeInnerAngle;
        m_ConeOuterAngle = other.m_ConeOuterAngle;
        m_ConeOuterGain = other.m_ConeOuterGain;
        m_ConeOuterGainHF = other.m_ConeOuterGainHF;
        return *this;
    }

    void AudioSourceComponent::OnInitialize()
    {
        AudioManager* audioManager = AudioManager::TryGet();
        if (audioManager == nullptr)
            return;
        if (m_Internal == nullptr)
            m_Internal = audioManager->CreateSource();
        ApplyRuntimeSettings();

        if (m_PlayOnAwake)
            m_Internal->Play();
    }

    void AudioSourceComponent::ApplyRuntimeSettings()
    {
        if (m_Internal == nullptr)
            return;

        m_Internal->SetClip(m_AudioClip);
        m_Internal->SetVolume(m_IsMuted ? 0.0f : m_Volume);
        m_Internal->SetPitch(m_Pitch);
        m_Internal->SetLooping(m_Loop);
        m_Internal->SetMinDistance(m_MinDistance);
        m_Internal->SetMaxDistance(m_MaxDistance);
        m_Internal->SetTime(m_Time);

        AudioManager* audioManager = AudioManager::TryGet();
        if (audioManager != nullptr)
        {
            Ref<AudioBus> bus = m_BusName.empty() ? nullptr : audioManager->FindBus(m_BusName);
            if (!bus && audioManager->GetActiveMixer())
                bus = audioManager->GetActiveMixer()->GetMasterBus();
            m_Internal->SetBus(bus);
        }

        m_Internal->SetLowPassGain(m_LowPassGain);
        m_Internal->SetHighPassGain(m_HighPassGain);
        m_Internal->SetConeInnerAngle(m_ConeInnerAngle);
        m_Internal->SetConeOuterAngle(m_ConeOuterAngle);
        m_Internal->SetConeOuterGain(m_ConeOuterGain);
        m_Internal->SetConeOuterGainHF(m_ConeOuterGainHF);
    }

    void AudioSourceComponent::OnTransformChanged(const Transform& transform)
    {
        if (m_Internal && m_Internal->Is3D())
            m_Internal->OnTransformChanged(transform);
    }

    void AudioSourceComponent::SetVolume(float volume)
    {
        if (m_Volume == volume)
            return;
        m_Volume = volume;
        if (m_Internal != nullptr)
            m_Internal->SetVolume(m_IsMuted ? 0.0f : m_Volume);
    }

    void AudioSourceComponent::SetPitch(float pitch)
    {
        if (m_Pitch == pitch)
            return;
        m_Pitch = pitch;
        if (m_Internal != nullptr)
            m_Internal->SetPitch(m_Pitch);
    }

    void AudioSourceComponent::SetClip(const AssetHandle<AudioClip>& clip)
    {
        if (m_AudioClip.GetUUID() == clip.GetUUID())
            return;
        m_AudioClip = clip;
        if (m_Internal != nullptr)
            m_Internal->SetClip(m_AudioClip);
    }

    void AudioSourceComponent::SetIsMuted(bool muted)
    {
        if (m_IsMuted == muted)
            return;
        m_IsMuted = muted;
        if (m_Internal != nullptr)
            m_Internal->SetVolume(m_IsMuted ? 0.0f : m_Volume);
    }

    void AudioSourceComponent::SetMinDistance(float minDistance)
    {
        if (m_MinDistance == minDistance)
            return;
        m_MinDistance = minDistance;
        if (m_Internal != nullptr)
            m_Internal->SetMinDistance(m_MinDistance);
    }

    void AudioSourceComponent::SetMaxDistance(float maxDistance)
    {
        if (m_MaxDistance == maxDistance)
            return;
        m_MaxDistance = maxDistance;
        if (m_Internal != nullptr)
            m_Internal->SetMaxDistance(m_MaxDistance);
    }

    void AudioSourceComponent::SetLooping(bool loop)
    {
        if (m_Loop == loop)
            return;
        m_Loop = loop;
        if (m_Internal != nullptr)
            m_Internal->SetLooping(m_Loop);
    }

    void AudioSourceComponent::SetTime(float time)
    {
        if (m_Time == time)
            return;
        m_Time = time;
        if (m_Internal != nullptr)
            m_Internal->SetTime(m_Time);
    }

    void AudioSourceComponent::SetPlayOnAwake(bool playOnAwake) { m_PlayOnAwake = playOnAwake; }

    void AudioSourceComponent::SetBusName(const String& busName)
    {
        if (m_BusName == busName)
            return;
        m_BusName = busName;
        if (m_Internal != nullptr)
        {
            AudioManager* audioManager = AudioManager::TryGet();
            if (audioManager == nullptr)
                return;
            Ref<AudioBus> bus = m_BusName.empty() ? nullptr : audioManager->FindBus(m_BusName);
            if (!bus && audioManager->GetActiveMixer())
                bus = audioManager->GetActiveMixer()->GetMasterBus();
            m_Internal->SetBus(bus);
        }
    }

    void AudioSourceComponent::SetLowPassGain(float gainHF)
    {
        if (m_LowPassGain == gainHF)
            return;
        m_LowPassGain = gainHF;
        if (m_Internal != nullptr)
            m_Internal->SetLowPassGain(m_LowPassGain);
    }

    void AudioSourceComponent::SetHighPassGain(float gainLF)
    {
        if (m_HighPassGain == gainLF)
            return;
        m_HighPassGain = gainLF;
        if (m_Internal != nullptr)
            m_Internal->SetHighPassGain(m_HighPassGain);
    }

    void AudioSourceComponent::SetConeInnerAngle(float degrees)
    {
        if (m_ConeInnerAngle == degrees)
            return;
        m_ConeInnerAngle = degrees;
        if (m_Internal != nullptr)
            m_Internal->SetConeInnerAngle(m_ConeInnerAngle);
    }

    void AudioSourceComponent::SetConeOuterAngle(float degrees)
    {
        if (m_ConeOuterAngle == degrees)
            return;
        m_ConeOuterAngle = degrees;
        if (m_Internal != nullptr)
            m_Internal->SetConeOuterAngle(m_ConeOuterAngle);
    }

    void AudioSourceComponent::SetConeOuterGain(float gain)
    {
        if (m_ConeOuterGain == gain)
            return;
        m_ConeOuterGain = gain;
        if (m_Internal != nullptr)
            m_Internal->SetConeOuterGain(m_ConeOuterGain);
    }

    void AudioSourceComponent::SetConeOuterGainHF(float gainHF)
    {
        if (m_ConeOuterGainHF == gainHF)
            return;
        m_ConeOuterGainHF = gainHF;
        if (m_Internal != nullptr)
            m_Internal->SetConeOuterGainHF(m_ConeOuterGainHF);
    }

    AudioSourceState AudioSourceComponent::GetState() const
    {
        if (m_Internal != nullptr)
            return m_Internal->GetState();
        return AudioSourceState::Stopped;
    }

    void AudioSourceComponent::Play()
    {
        if (m_Internal != nullptr)
            m_Internal->Play();
    }

    void AudioSourceComponent::Pause()
    {
        if (m_Internal != nullptr)
            m_Internal->Pause();
    }

    void AudioSourceComponent::Stop()
    {
        if (m_Internal != nullptr)
            m_Internal->Stop();
    }

    TextComponent::TextComponent() : ComponentBase() { /* Font = FontManager::Get("default"); */ }

    float Rigidbody2DComponent::GetMass() const { return m_Mass; }

    glm::vec2 Rigidbody2DComponent::GetCenterOfMass() const { return m_CenterOfMass; }

    void Rigidbody2DComponent::SetLayerMask(uint32_t layerMask, Entity e)
    {
        m_LayerMask = std::min(layerMask, Physics2DLayerCount - 1);
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateLayer(e);
    }

    float Rigidbody2DComponent::GetInertia() const { return m_Inertia; }

    void Rigidbody2DComponent::SetBodyType(RigidbodyBodyType bodyType)
    {
        m_Type = bodyType;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateBodyType(*this);
    }

    void Rigidbody2DComponent::SetMass(float mass)
    {
        m_Mass = std::max(mass, 0.0001f);
        if (!m_AutoMass && Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateMass(*this, m_Mass);
    }

    void Rigidbody2DComponent::SetGravityScale(float scale)
    {
        m_GravityScale = scale;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateGravityScale(*this, scale);
    }

    void Rigidbody2DComponent::SetConstraints(Rigidbody2DConstraints constraints)
    {
        m_Constraints = constraints;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateConstraints(*this);
    }

    void Rigidbody2DComponent::SetCollisionDetectionMode(CollisionDetectionMode2D value)
    {
        m_ContinuousCollisionDetection = value;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateCollisionDetectionMode(*this);
    }

    void Rigidbody2DComponent::SetSleepMode(RigidbodySleepMode sleepMode)
    {
        m_SleepMode = sleepMode;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateSleepMode(*this);
    }

    void Rigidbody2DComponent::SetLinearDrag(float linearDrag)
    {
        m_LinearDrag = std::max(linearDrag, 0.0f);
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateLinearDrag(*this, m_LinearDrag);
    }

    void Rigidbody2DComponent::SetAngularDrag(float angularDrag)
    {
        m_AngularDrag = std::max(angularDrag, 0.0f);
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateAngularDrag(*this, m_AngularDrag);
    }

    void Rigidbody2DComponent::SetAutoMass(bool autoMass, Entity entity)
    {
        m_AutoMass = autoMass;
        if (!Physics2D::IsStartedUp())
            return;
        if (autoMass)
        {
            Physics2D::TryGet()->ResetMass(entity);
            m_Mass = Physics2D::TryGet()->CalculateMass(entity);
        }
        else
            Physics2D::TryGet()->UpdateMass(*this, m_Mass);
    }

    void Rigidbody2DComponent::SetCenterOfMass(const glm::vec2& center)
    {
        m_CenterOfMass = center;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateCenterOfMass(*this, center);
    }

    void Rigidbody2DComponent::SetInterpolationMode(RigidbodyInterpolation interpolation) { m_InterpolationMode = interpolation; }

    void Rigidbody2DComponent::SetInertia(float inertia)
    {
        m_Inertia = std::max(inertia, 0.0f);
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateInertia(*this, m_Inertia);
    }

    void Collider2D::SetIsTrigger(bool trigger)
    {
        m_IsTrigger = trigger;
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateTrigger(*this, trigger);
    }

    void Collider2D::SetMaterial(const AssetHandle<PhysicsMaterial2D>& material)
    {
        m_Material = material.HasUUID() ? material : (Physics2D::IsStartedUp() ? Physics2D::TryGet()->GetDefaultMaterial() : material);
        RefreshMaterial();
    }

    const PhysicsMaterialData& Collider2D::GetMaterialData() const
    {
        if (m_Material)
            return m_Material->GetData();
        if (Physics2D::IsStartedUp() && Physics2D::TryGet()->GetDefaultMaterial())
            return Physics2D::TryGet()->GetDefaultMaterial()->GetData();
        static const PhysicsMaterialData fallback;
        return fallback;
    }

    void Collider2D::RefreshMaterial()
    {
        if (Physics2D::IsStartedUp())
            Physics2D::TryGet()->UpdateMaterial(*this);
    }

    BoxCollider2DComponent::BoxCollider2DComponent() : Collider2D()
    {
        if (Physics2D::IsStartedUp())
            m_Material = Physics2D::TryGet()->GetDefaultMaterial();
    }

    void BoxCollider2DComponent::SetOffset(const glm::vec2& offset, Entity entity)
    {
        m_Offset = offset;
        if (RuntimeFixture != nullptr)
        {
            Physics2D::TryGet()->DestroyFixture(entity, *this);
            Physics2D::TryGet()->CreateBoxCollider(entity);
        }
    }

    void BoxCollider2DComponent::SetSize(const glm::vec2& size, Entity entity)
    {
        m_Size = size;
        if (RuntimeFixture != nullptr)
        {
            Physics2D::TryGet()->DestroyFixture(entity, *this);
            Physics2D::TryGet()->CreateBoxCollider(entity);
        }
    }

    CircleCollider2DComponent::CircleCollider2DComponent() : Collider2D()
    {
        if (Physics2D::IsStartedUp())
            m_Material = Physics2D::TryGet()->GetDefaultMaterial();
    }

    void CircleCollider2DComponent::SetRadius(float radius, Entity entity)
    {
        m_Radius = radius;
        if (RuntimeFixture != nullptr)
        {
            Physics2D::TryGet()->DestroyFixture(entity, *this);
            Physics2D::TryGet()->CreateCircleCollider(entity);
        }
    }

    void CircleCollider2DComponent::SetOffset(const glm::vec2& offset, Entity entity)
    {
        m_Offset = offset;
        if (RuntimeFixture != nullptr)
        {
            Physics2D::TryGet()->DestroyFixture(entity, *this);
            Physics2D::TryGet()->CreateCircleCollider(entity);
        }
    }

    ManagedScript::ManagedScript(ScriptTypeIdentity identity)
      : m_Identity(std::move(identity)), InstanceId(s_NextAvailableId.fetch_add(1, std::memory_order_relaxed)),
        m_State(MakeEmptyScriptState(m_Identity))
    {
        CW_ENGINE_ASSERT(m_Identity.IsValid());
    }

    ManagedScript::ManagedScript(const ManagedScript& other)
      : InstanceId(s_NextAvailableId.fetch_add(1, std::memory_order_relaxed)), m_Identity(other.m_Identity), m_State(other.m_State)
    {
    }

    ManagedScript& ManagedScript::operator=(const ManagedScript& other)
    {
        if (this == &other)
            return *this;

        m_Identity = other.m_Identity;
        m_State = other.m_State;
        m_RuntimeHandle = {};
        return *this;
    }

    bool ManagedScript::SetState(ScriptState state)
    {
        if (state.Identity != m_Identity)
        {
            CW_ENGINE_WARN("Cannot apply state for '{}:{}' to script '{}:{}'.", state.Identity.Assembly, state.Identity.GetFullName(), m_Identity.Assembly,
                           m_Identity.GetFullName());
            return false;
        }
        m_State = std::move(state);
        return true;
    }

    ManagedScript* ManagedScriptComponent::FindScript(uint64_t runtimeInstanceId)
    {
        const auto script = std::find_if(Scripts.begin(), Scripts.end(),
                                         [runtimeInstanceId](const ManagedScript& candidate) { return candidate.InstanceId == runtimeInstanceId; });
        return script == Scripts.end() ? nullptr : &*script;
    }

    const ManagedScript* ManagedScriptComponent::FindScript(uint64_t runtimeInstanceId) const
    {
        const auto script = std::find_if(Scripts.begin(), Scripts.end(),
                                         [runtimeInstanceId](const ManagedScript& candidate) { return candidate.InstanceId == runtimeInstanceId; });
        return script == Scripts.end() ? nullptr : &*script;
    }

    void Rigidbody3DComponent::CopySettings(const Rigidbody3DComponent& other)
    {
        m_Type = other.m_Type;
        m_Mass = other.m_Mass;
        m_AutoMass = other.m_AutoMass;
        m_GravityScale = other.m_GravityScale;
        m_LinearDamping = other.m_LinearDamping;
        m_AngularDamping = other.m_AngularDamping;
        m_CenterOfMass = other.m_CenterOfMass;
        m_AllowSleep = other.m_AllowSleep;
        m_StartAwake = other.m_StartAwake;
        m_ContinuousCollision = other.m_ContinuousCollision;
        m_LockRotationX = other.m_LockRotationX;
        m_LockRotationY = other.m_LockRotationY;
        m_LockRotationZ = other.m_LockRotationZ;
        m_Filter = other.m_Filter;
        m_LinearVelocity = other.m_LinearVelocity;
        m_AngularVelocity = other.m_AngularVelocity;
    }

    Rigidbody3DComponent::Rigidbody3DComponent(const Rigidbody3DComponent& other) : ComponentBase(other) { CopySettings(other); }

    Rigidbody3DComponent& Rigidbody3DComponent::operator=(const Rigidbody3DComponent& other)
    {
        if (this != &other)
        {
            ComponentBase::operator=(other);
            CopySettings(other);
        }
        return *this;
    }

    void Rigidbody3DComponent::SetBodyType(PhysicsBodyType3D type, Entity entity)
    {
        if (m_Type == type)
            return;
        m_Type = type;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetMass(float mass, Entity entity)
    {
        mass = std::max(mass, 0.0001f);
        if (m_Mass == mass)
            return;
        m_Mass = mass;
        if (entity && !m_AutoMass)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetAutoMass(bool autoMass, Entity entity)
    {
        if (m_AutoMass == autoMass)
            return;
        m_AutoMass = autoMass;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetGravityScale(float scale)
    {
        m_GravityScale = scale;
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetGravityScale(RuntimeBody, scale);
    }

    void Rigidbody3DComponent::SetDamping(float linear, float angular)
    {
        m_LinearDamping = std::max(linear, 0.0f);
        m_AngularDamping = std::max(angular, 0.0f);
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetDamping(RuntimeBody, m_LinearDamping, m_AngularDamping);
    }

    void Rigidbody3DComponent::SetCenterOfMass(const glm::vec3& center, Entity entity)
    {
        m_CenterOfMass = center;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetAllowSleep(bool allowSleep, Entity entity)
    {
        if (m_AllowSleep == allowSleep)
            return;
        m_AllowSleep = allowSleep;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetStartAwake(bool startAwake, Entity entity)
    {
        m_StartAwake = startAwake;
        if (entity && RuntimeBody)
            SetAwake(startAwake);
    }

    void Rigidbody3DComponent::SetContinuousCollision(bool continuous, Entity entity)
    {
        if (m_ContinuousCollision == continuous)
            return;
        m_ContinuousCollision = continuous;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetRotationLocks(bool x, bool y, bool z, Entity entity)
    {
        if (m_LockRotationX == x && m_LockRotationY == y && m_LockRotationZ == z)
            return;
        m_LockRotationX = x;
        m_LockRotationY = y;
        m_LockRotationZ = z;
        if (entity)
            entity.GetScene()->RecreatePhysics3DBody(entity);
    }

    void Rigidbody3DComponent::SetFilter(const PhysicsFilter3D& filter)
    {
        m_Filter = filter;
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetFilter(RuntimeBody, filter);
    }

    glm::vec3 Rigidbody3DComponent::GetLinearVelocity() const
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            return Physics3D::Get().GetLinearVelocity(RuntimeBody);
        return m_LinearVelocity;
    }

    glm::vec3 Rigidbody3DComponent::GetAngularVelocity() const
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            return Physics3D::Get().GetAngularVelocity(RuntimeBody);
        return m_AngularVelocity;
    }

    void Rigidbody3DComponent::SetLinearVelocity(const glm::vec3& velocity)
    {
        m_LinearVelocity = velocity;
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetLinearVelocity(RuntimeBody, velocity);
    }

    void Rigidbody3DComponent::SetAngularVelocity(const glm::vec3& velocity)
    {
        m_AngularVelocity = velocity;
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetAngularVelocity(RuntimeBody, velocity);
    }

    void Rigidbody3DComponent::AddForce(const glm::vec3& force, PhysicsForceMode3D mode)
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().AddForce(RuntimeBody, force, mode);
    }

    void Rigidbody3DComponent::AddForceAt(const glm::vec3& force, const glm::vec3& point, PhysicsForceMode3D mode)
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().AddForceAt(RuntimeBody, force, point, mode);
    }

    void Rigidbody3DComponent::AddTorque(const glm::vec3& torque, PhysicsForceMode3D mode)
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().AddTorque(RuntimeBody, torque, mode);
    }

    void Rigidbody3DComponent::SetAwake(bool awake)
    {
        if (RuntimeBody && Physics3D::IsStartedUp())
            Physics3D::Get().SetAwake(RuntimeBody, awake);
    }

    bool Rigidbody3DComponent::IsAwake() const { return RuntimeBody && Physics3D::IsStartedUp() && Physics3D::Get().IsAwake(RuntimeBody); }

    Collider3D::Collider3D() : ComponentBase()
    {
        if (Physics3D::IsStartedUp())
            m_Material = Physics3D::Get().GetDefaultMaterial();
    }

    void Collider3D::CopySettings(const Collider3D& other)
    {
        m_Offset = other.m_Offset;
        m_Rotation = other.m_Rotation;
        m_IsTrigger = other.m_IsTrigger;
        m_Material = other.m_Material;
        m_Filter = other.m_Filter;
    }

    Collider3D::Collider3D(const Collider3D& other) : ComponentBase(other) { CopySettings(other); }

    Collider3D& Collider3D::operator=(const Collider3D& other)
    {
        if (this != &other)
        {
            ComponentBase::operator=(other);
            CopySettings(other);
        }
        return *this;
    }

    void Collider3D::SetOffset(const glm::vec3& offset, Entity entity)
    {
        m_Offset = offset;
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    void Collider3D::SetRotation(const glm::quat& rotation, Entity entity)
    {
        m_Rotation = glm::normalize(rotation);
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    void Collider3D::SetIsTrigger(bool trigger)
    {
        m_IsTrigger = trigger;
        if (RuntimeShape && Physics3D::IsStartedUp())
            Physics3D::Get().SetShapeTrigger(RuntimeShape, trigger);
    }

    const PhysicsMaterialData& Collider3D::GetMaterialData() const
    {
        if (m_Material)
            return m_Material->GetData();
        if (Physics3D::IsStartedUp() && Physics3D::Get().GetDefaultMaterial())
            return Physics3D::Get().GetDefaultMaterial()->GetData();
        static const PhysicsMaterialData fallback;
        return fallback;
    }

    void Collider3D::SetMaterial(const AssetHandle<PhysicsMaterial3D>& material)
    {
        m_Material = material.HasUUID() ? material : (Physics3D::IsStartedUp() ? Physics3D::Get().GetDefaultMaterial() : material);
        RefreshMaterial();
    }

    void Collider3D::RefreshMaterial()
    {
        if (RuntimeShape && Physics3D::IsStartedUp())
            Physics3D::Get().SetShapeMaterial(RuntimeShape, GetMaterialData());
    }

    void Collider3D::SetFilter(const PhysicsFilter3D& filter, Entity entity)
    {
        m_Filter = filter;
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    BoxCollider3DComponent::BoxCollider3DComponent(const BoxCollider3DComponent& other) : Collider3D(other), m_Size(other.m_Size) {}

    BoxCollider3DComponent& BoxCollider3DComponent::operator=(const BoxCollider3DComponent& other)
    {
        if (this != &other)
        {
            Collider3D::operator=(other);
            m_Size = other.m_Size;
        }
        return *this;
    }

    void BoxCollider3DComponent::SetSize(const glm::vec3& size, Entity entity)
    {
        m_Size = glm::max(glm::abs(size), glm::vec3(0.001f));
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    SphereCollider3DComponent::SphereCollider3DComponent(const SphereCollider3DComponent& other) : Collider3D(other), m_Radius(other.m_Radius) {}

    SphereCollider3DComponent& SphereCollider3DComponent::operator=(const SphereCollider3DComponent& other)
    {
        if (this != &other)
        {
            Collider3D::operator=(other);
            m_Radius = other.m_Radius;
        }
        return *this;
    }

    void SphereCollider3DComponent::SetRadius(float radius, Entity entity)
    {
        m_Radius = std::max(radius, 0.001f);
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    CapsuleCollider3DComponent::CapsuleCollider3DComponent(const CapsuleCollider3DComponent& other)
      : Collider3D(other), m_Radius(other.m_Radius), m_Height(other.m_Height)
    {
    }

    CapsuleCollider3DComponent& CapsuleCollider3DComponent::operator=(const CapsuleCollider3DComponent& other)
    {
        if (this != &other)
        {
            Collider3D::operator=(other);
            m_Radius = other.m_Radius;
            m_Height = other.m_Height;
        }
        return *this;
    }

    void CapsuleCollider3DComponent::SetRadius(float radius, Entity entity)
    {
        m_Radius = std::max(radius, 0.001f);
        m_Height = std::max(m_Height, m_Radius * 2.0f);
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

    void CapsuleCollider3DComponent::SetHeight(float height, Entity entity)
    {
        m_Height = std::max(height, m_Radius * 2.0f);
        if (entity)
            entity.GetScene()->RecreatePhysics3DShapes(entity);
    }

} // namespace Crowny
