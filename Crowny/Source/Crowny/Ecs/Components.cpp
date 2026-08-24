#include "cwpch.h"

#include <mono/metadata/object.h>

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

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

namespace Crowny
{
    RelationshipComponent& RelationshipComponent::operator=(const RelationshipComponent& other)
    {
        if (this == &other)
            return *this;
        ComponentBase::operator=(other);
        Parent = {};
        Children.clear();
        return *this;
    }

    void AudioListenerComponent::Initialize() { m_Internal = AudioManager::TryGet()->CreateListener(); }

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
        m_Internal = nullptr;
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
        if (m_Internal == nullptr)
            m_Internal = AudioManager::TryGet()->CreateSource();
        m_Internal->SetClip(m_AudioClip);
        m_Internal->SetVolume(m_Volume);
        m_Internal->SetPitch(m_Pitch);
        m_Internal->SetLooping(m_Loop);
        m_Internal->SetMinDistance(m_MinDistance);
        m_Internal->SetMaxDistance(m_MaxDistance);
        m_Internal->SetTime(m_Time);

        // Resolve bus name against the active mixer. Empty name = leave the source on whatever bus
        // CreateSource() defaulted to (the master bus of the active mixer, or nothing).
        if (!m_BusName.empty())
        {
            if (Ref<AudioBus> bus = AudioManager::TryGet()->FindBus(m_BusName))
                m_Internal->SetBus(bus);
        }

        m_Internal->SetLowPassGain(m_LowPassGain);
        m_Internal->SetHighPassGain(m_HighPassGain);
        m_Internal->SetConeInnerAngle(m_ConeInnerAngle);
        m_Internal->SetConeOuterAngle(m_ConeOuterAngle);
        m_Internal->SetConeOuterGain(m_ConeOuterGain);
        m_Internal->SetConeOuterGainHF(m_ConeOuterGainHF);

        if (m_PlayOnAwake)
            m_Internal->Play();
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
            m_Internal->SetVolume(m_Volume);
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
            Ref<AudioBus> bus = m_BusName.empty() ? nullptr : AudioManager::TryGet()->FindBus(m_BusName);
            if (!bus && AudioManager::TryGet()->GetActiveMixer())
                bus = AudioManager::TryGet()->GetActiveMixer()->GetMasterBus();
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

    MonoScript::MonoScript() : m_Identity{ GAME_ASSEMBLY, {}, {} }, InstanceId(s_NextAvailableId++) {}

    MonoScript::MonoScript(ScriptTypeIdentity identity) : m_Identity(std::move(identity)), InstanceId(s_NextAvailableId++) {}

    MonoScript::MonoScript(MonoReflectionType* runtimeType) : MonoScript(GAME_ASSEMBLY, runtimeType) {}

    MonoScript::MonoScript(const String& assemblyName, MonoReflectionType* runtimeType)
      : m_Identity{ assemblyName, {}, {} }, m_RuntimeType(runtimeType), InstanceId(s_NextAvailableId++)
    {
        MonoUtils::GetClassName(runtimeType, m_Identity.Namespace, m_Identity.TypeName);
        if (MonoManager::IsStartedUp())
        {
            MonoAssembly* assembly = MonoManager::Get().FindAssembly(MonoUtils::GetClass(runtimeType));
            if (assembly != nullptr)
                m_Identity.Assembly = assembly->GetName();
        }
    }

    MonoScript::MonoScript(const MonoScript& other)
      : InstanceId(s_NextAvailableId.fetch_add(1, std::memory_order_relaxed)), m_Identity(other.m_Identity),
        m_MissingType(other.m_MissingType), m_SerializedObjectData(other.CapturePersistedState().Fields)
    {
    }

    MonoScript& MonoScript::operator=(const MonoScript& other)
    {
        if (this == &other)
            return *this;

        m_Identity = other.m_Identity;
        m_MissingType = other.m_MissingType;
        m_SerializedObjectData = other.CapturePersistedState().Fields;
        m_ObjectInfo = nullptr;
        m_RuntimeType = nullptr;
        m_Class = nullptr;
        m_ScriptEntityBehaviour = nullptr;
        ResetRuntimeCallbacks();
        return *this;
    }

    PersistedScriptState MonoScript::CapturePersistedState() const
    {
        PersistedScriptState persisted{ m_Identity, m_SerializedObjectData };
        if (m_ScriptEntityBehaviour == nullptr || m_MissingType)
            return persisted;

        MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
        if (instance == nullptr || m_ObjectInfo == nullptr)
            return persisted;

        Ref<SerializableObject> state = SerializableObject::CreateFromMonoObject(instance, m_ObjectInfo);
        if (state != nullptr)
        {
            state->Serialize();
            persisted.Fields = state;
        }
        return persisted;
    }

    bool MonoScript::ApplyPersistedState(const PersistedScriptState& state)
    {
        if (state.Identity != m_Identity)
        {
            CW_ENGINE_WARN("Cannot apply persisted state for '{}:{}' to script '{}:{}'.", state.Identity.Assembly,
                           state.Identity.GetFullName(), m_Identity.Assembly, m_Identity.GetFullName());
            return false;
        }

        MonoObject* instance = GetManagedInstance();
        if (!m_MissingType && instance != nullptr && m_ObjectInfo != nullptr)
        {
            if (state.Fields != nullptr)
                state.Fields->Deserialize(instance, m_ObjectInfo);
            m_SerializedObjectData = nullptr;
            return true;
        }

        m_SerializedObjectData = state.Fields;
        return true;
    }

    bool MonoScript::ResolveObjectInfo()
    {
        m_ObjectInfo = nullptr;
        m_Class = nullptr;
        m_RuntimeType = nullptr;
        if (!m_Identity.IsValid() || !MonoManager::IsStartedUp() || !ScriptInfoManager::IsStartedUp())
            return false;

        m_Class = MonoManager::Get().FindClass(m_Identity.Assembly, m_Identity.Namespace, m_Identity.TypeName);
        if (m_Class == nullptr || !ScriptInfoManager::Get().GetSerializableObjectInfo(
                                  m_Identity.Assembly, m_Identity.Namespace, m_Identity.TypeName, m_ObjectInfo))
        {
            m_ObjectInfo = nullptr;
            m_Class = nullptr;
            return false;
        }

        m_RuntimeType = MonoUtils::GetType(m_Class->GetInternalPtr());
        return true;
    }

    void MonoScript::ResetRuntimeCallbacks()
    {
        m_OnStartThunk = nullptr;
        m_OnUpdateThunk = nullptr;
        m_OnDestroyThunk = nullptr;
        m_OnCollisionEnterThunk = nullptr;
        m_OnCollisionStayThunk = nullptr;
        m_OnCollisionExitThunk = nullptr;
        m_OnTriggerEnterThunk = nullptr;
        m_OnTriggerStayThunk = nullptr;
        m_OnTriggerExitThunk = nullptr;
        m_OnCollisionEnter3DThunk = nullptr;
        m_OnCollisionStay3DThunk = nullptr;
        m_OnCollisionExit3DThunk = nullptr;
        m_OnTriggerEnter3DThunk = nullptr;
        m_OnTriggerStay3DThunk = nullptr;
        m_OnTriggerExit3DThunk = nullptr;
    }

    void MonoScript::ClearRuntimeInstance()
    {
        m_ScriptEntityBehaviour = nullptr;
        m_ObjectInfo = nullptr;
        m_RuntimeType = nullptr;
        m_Class = nullptr;
        ResetRuntimeCallbacks();
    }

    MonoClass* MonoScript::GetManagedClass() const { return m_Class; }
    MonoObject* MonoScript::GetManagedInstance() const
    {
        return m_ScriptEntityBehaviour != nullptr ? m_ScriptEntityBehaviour->GetManagedInstance() : nullptr;
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
        RuntimeBody = {};
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
        RuntimeShape = {};
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

    void MonoScript::Create(Entity entity)
    {
        if (!entity || !m_Identity.IsValid())
        {
            CW_ENGINE_WARN("Cannot create managed script with invalid persisted identity '{}:{}'.", m_Identity.Assembly,
                           m_Identity.GetFullName());
            return;
        }
        if (!MonoManager::IsStartedUp() || !ScriptInfoManager::IsStartedUp() || !ScriptSceneObjectManager::IsStartedUp())
        {
            CW_ENGINE_WARN("Managed script '{}:{}' remains retained because the scripting runtime is unavailable.", m_Identity.Assembly,
                           m_Identity.GetFullName());
            return;
        }

        if (m_ScriptEntityBehaviour != nullptr)
            ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, this);

        MonoObject* instance = nullptr;
        if (!ResolveObjectInfo())
        {
            m_MissingType = true;
            MonoClass* missingClass = ScriptInfoManager::Get().GetBuiltinClasses().MissingEntityBehaviour;
            if (missingClass == nullptr)
            {
                CW_ENGINE_WARN("Managed script type '{}:{}' is unavailable. Its persisted fields were retained.", m_Identity.Assembly,
                               m_Identity.GetFullName());
                return;
            }
            instance = missingClass->CreateInstance(true);
            CW_ENGINE_WARN("Managed script type '{}:{}' is unavailable. Its persisted fields were retained.", m_Identity.Assembly,
                           m_Identity.GetFullName());
        }
        else
        {
            instance = m_ObjectInfo->m_MonoClass->CreateInstance(true);
            m_MissingType = false;
        }

        ScriptSceneObjectManager::Get().CreateManagedScriptComponent(instance, entity, *this);

        ApplyPersistedState({ m_Identity, m_SerializedObjectData });
    }

    void MonoScript::OnInitialize(ScriptEntityBehaviour* entityBehaviour)
    {
        m_ScriptEntityBehaviour = entityBehaviour;
        if (!ResolveObjectInfo())
        {
            m_MissingType = true;
            ResetRuntimeCallbacks();
            return;
        }
        m_MissingType = false;

        MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
        if (instance != nullptr)
            m_RuntimeType = MonoUtils::GetType(MonoUtils::GetClass(instance));

        ResetRuntimeCallbacks();

        MonoClass* currentClass = m_Class;
        while (currentClass != nullptr)
        {
            if (m_OnStartThunk == nullptr)
            {
                MonoMethod* onStartMethod = currentClass->GetMethod("Start", 0);
                if (onStartMethod != nullptr)
                    m_OnStartThunk = (LifecycleThunk)onStartMethod->GetThunk();
            }

            if (m_OnUpdateThunk == nullptr)
            {
                MonoMethod* onUpdateMethod = currentClass->GetMethod("Update", 0);
                if (onUpdateMethod != nullptr)
                    m_OnUpdateThunk = (LifecycleThunk)onUpdateMethod->GetThunk();
            }

            if (m_OnDestroyThunk == nullptr)
            {
                MonoMethod* onDestroyMethod = currentClass->GetMethod("Destroy", 0);
                if (onDestroyMethod != nullptr)
                    m_OnDestroyThunk = (LifecycleThunk)onDestroyMethod->GetThunk();
            }

            if (m_OnCollisionEnterThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionEnter2D", "Collision2D");
                if (method != nullptr)
                    m_OnCollisionEnterThunk = (OnCollisionEnterThunkDef)method->GetThunk();
            }
            if (m_OnCollisionStayThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionStay2D", "Collision2D");
                if (method != nullptr)
                    m_OnCollisionStayThunk = (OnCollisionStayThunkDef)method->GetThunk();
            }
            if (m_OnCollisionExitThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionExit2D", "Collision2D");
                if (method != nullptr)
                    m_OnCollisionExitThunk = (OnCollisionExitThunkDef)method->GetThunk();
            }
            if (m_OnTriggerEnterThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerEnter2D", "Entity");
                if (method != nullptr)
                    m_OnTriggerEnterThunk = (OnTriggerEnterThunkDef)method->GetThunk();
            }
            if (m_OnTriggerStayThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerStay2D", "Entity");
                if (method != nullptr)
                    m_OnTriggerStayThunk = (OnTriggerStayThunkDef)method->GetThunk();
            }
            if (m_OnTriggerExitThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerExit2D", "Entity");
                if (method != nullptr)
                    m_OnTriggerExitThunk = (OnTriggerExitThunkDef)method->GetThunk();
            }
            if (m_OnCollisionEnter3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionEnter3D", "Collision3D");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnCollisionEnter", "Collision3D");
                if (method != nullptr)
                    m_OnCollisionEnter3DThunk = (OnCollisionEnterThunkDef)method->GetThunk();
            }
            if (m_OnCollisionStay3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionStay3D", "Collision3D");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnCollisionStay", "Collision3D");
                if (method != nullptr)
                    m_OnCollisionStay3DThunk = (OnCollisionStayThunkDef)method->GetThunk();
            }
            if (m_OnCollisionExit3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnCollisionExit3D", "Collision3D");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnCollisionExit", "Collision3D");
                if (method != nullptr)
                    m_OnCollisionExit3DThunk = (OnCollisionExitThunkDef)method->GetThunk();
            }
            if (m_OnTriggerEnter3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerEnter3D", "Entity");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnTriggerEnter", "Entity");
                if (method != nullptr)
                    m_OnTriggerEnter3DThunk = (OnTriggerEnterThunkDef)method->GetThunk();
            }
            if (m_OnTriggerStay3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerStay3D", "Entity");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnTriggerStay", "Entity");
                if (method != nullptr)
                    m_OnTriggerStay3DThunk = (OnTriggerStayThunkDef)method->GetThunk();
            }
            if (m_OnTriggerExit3DThunk == nullptr)
            {
                MonoMethod* method = currentClass->GetMethod("OnTriggerExit3D", "Entity");
                if (method == nullptr)
                    method = currentClass->GetMethod("OnTriggerExit", "Entity");
                if (method != nullptr)
                    m_OnTriggerExit3DThunk = (OnTriggerExitThunkDef)method->GetThunk();
            }

            MonoClass* baseClass = currentClass->GetBaseClass();
            if (baseClass == ScriptEntityBehaviour::GetMetaData()->ScriptClass)
                break;
            currentClass = baseClass;
        }
        // Could add and call an OnAwake method like in Unity here

        MonoClass* requireClass = ScriptInfoManager::Get().GetBuiltinClasses().RequireComponentAttribute;
        MonoObject* requireComponent = m_ObjectInfo->m_TypeInfo->GetAttribute(requireClass);
        if (requireComponent != nullptr)
        {
            MonoField* field = requireClass->GetField("components");
            MonoObject* components = nullptr;
            components = field->GetBoxed(requireComponent);
            if (components != nullptr)
            {
                MonoClass* listClass = field->GetType();
                MonoProperty* countProp = listClass->GetProperty("Count");
                MonoObject* lengthObj = countProp->Get(components);
                const uint32_t length = *(int32_t*)MonoUtils::Unbox(lengthObj);
                MonoProperty* itemProp = listClass->GetProperty("Item");
                for (uint32_t i = 0; i < length; i++)
                {
                    MonoReflectionType* reflType = (MonoReflectionType*)itemProp->GetIndexed(components, i);
                    ComponentInfo* componentInfo = ScriptInfoManager::Get().GetComponentInfo(reflType);
                    if (componentInfo != nullptr)
                    {
                        if (!componentInfo->HasCallback(entityBehaviour->GetNativeEntity()))
                            componentInfo->AddCallback(entityBehaviour->GetNativeEntity());
                    }
                    else
                        CW_ENGINE_WARN("Could not find component class {0} used in RequireComponent for class {1}",
                                       MonoUtils::GetReflTypeName(reflType), m_Class->GetFullName());
                }
            }
        }
    }

    ScriptObjectBackupData MonoScript::Backup()
    {
        ScriptObjectBackupData data{ CapturePersistedState().Fields };
        ResetRuntimeCallbacks();
        return data;
    }

    void MonoScript::Restore(const ScriptObjectBackupData& data)
    {
        OnInitialize(m_ScriptEntityBehaviour);
        ApplyPersistedState({ m_Identity, data.SerializedObject });
    }

    void MonoScript::SetClassName(const String& className)
    {
        const size_t namespaceSeparator = className.find_last_of('.');
        ScriptTypeIdentity identity = m_Identity;
        if (identity.Assembly.empty())
            identity.Assembly = GAME_ASSEMBLY;
        if (namespaceSeparator == String::npos)
        {
            identity.Namespace = "Sandbox";
            identity.TypeName = className;
        }
        else
        {
            identity.Namespace = className.substr(0, namespaceSeparator);
            identity.TypeName = className.substr(namespaceSeparator + 1);
        }
        if (identity != m_Identity)
            m_SerializedObjectData = nullptr;
        m_Identity = std::move(identity);
        m_MissingType = !ResolveObjectInfo();
        ResetRuntimeCallbacks();
    }

    void MonoScript::OnStart()
    {
        GetStartCallback().Invoke();
    }

    void MonoScript::OnUpdate()
    {
        GetUpdateCallback().Invoke();
    }

    void MonoScript::OnDestroy()
    {
        GetDestroyCallback().Invoke();
    }

    void MonoScript::RuntimeCallback::Invoke() const
    {
        if (*this)
            MonoUtils::InvokeThunk(Thunk, Instance);
    }

    MonoScript::RuntimeCallback MonoScript::GetStartCallback() const
    {
        return { GetManagedInstance(), m_OnStartThunk };
    }

    MonoScript::RuntimeCallback MonoScript::GetUpdateCallback() const
    {
        return { GetManagedInstance(), m_OnUpdateThunk };
    }

    MonoScript::RuntimeCallback MonoScript::GetDestroyCallback() const
    {
        return { GetManagedInstance(), m_OnDestroyThunk };
    }

    ScriptObjectBackupData MonoScriptComponent::Backup(bool clearExisting)
    {
        for (auto& script : Scripts)
            return script.Backup();
        return {};
    }

    void MonoScriptComponent::Restore(const ScriptObjectBackupData& data)
    {
        for (auto& script : Scripts)
            script.Restore(data);
    }

    struct CollisionDataInterop
    {
        MonoArray* Colliders;
        MonoArray* ContactPoints;
    };

    static CollisionDataInterop CollisionDataToManaged(const Collision2D& collision)
    {
        CollisionDataInterop output;
        MonoArray* colliders = mono_array_new(MonoManager::Get().GetDomain(), ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(), 2);

        ScriptEntity* col1 = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(collision.Colliders[0]);
        if (col1 != nullptr)
            mono_array_setref(colliders, 0, col1->GetManagedInstance());
        ScriptEntity* col2 = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(collision.Colliders[1]);
        if (col2 != nullptr)
            mono_array_setref(colliders, 1, col2->GetManagedInstance());

        output.Colliders = colliders;

        ::MonoClass* vecClass = MonoManager::Get().FindClass("Crowny", "Vector2")->GetInternalPtr();
        MonoArray* points = mono_array_new(MonoManager::Get().GetDomain(), vecClass, collision.Points.size());
        for (uint32_t i = 0; i < collision.Points.size(); i++)
            mono_array_set(points, glm::vec2, i, collision.Points[i]);
        output.ContactPoints = points;
        return output;
    };

    void MonoScript::OnCollisionEnter2D(const Collision2D& collision)
    {
        if (m_OnCollisionEnterThunk != nullptr)
        {
            const CollisionDataInterop data = CollisionDataToManaged(collision);
            MonoObject* managedCollision = MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnCollisionEnterThunk, instance, managedCollision);
        }
    }

    void MonoScript::OnCollisionStay2D(const Collision2D& collision)
    {
        if (m_OnCollisionStayThunk != nullptr)
        {
            const CollisionDataInterop data = CollisionDataToManaged(collision);
            MonoObject* managedCollision = MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnCollisionStayThunk, instance, managedCollision);
        }
    }

    void MonoScript::OnCollisionExit2D(const Collision2D& collision)
    {
        if (m_OnCollisionExitThunk != nullptr)
        {
            const CollisionDataInterop data = CollisionDataToManaged(collision);
            MonoObject* managedCollision = MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnCollisionExitThunk, instance, managedCollision);
        }
    }

    void MonoScript::OnTriggerEnter2D(Entity other)
    {
        if (m_OnTriggerEnterThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnTriggerEnterThunk, instance,
                                   ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(other)->GetManagedInstance());
        }
    }

    void MonoScript::OnTriggerStay2D(Entity other)
    {
        if (m_OnTriggerStayThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnTriggerStayThunk, instance,
                                   ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(other)->GetManagedInstance());
        }
    }

    void MonoScript::OnTriggerExit2D(Entity other)
    {
        if (m_OnTriggerExitThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnTriggerExitThunk, instance,
                                   ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(other)->GetManagedInstance());
        }
    }

    struct Collision3DInterop
    {
        MonoArray* Colliders;
        MonoArray* Contacts;
    };

    struct ContactPoint3DInterop
    {
        glm::vec3 Point;
        glm::vec3 Normal;
        float Separation;
        float NormalImpulse;
    };

    static Collision3DInterop CollisionDataToManaged(const Collision3D& collision)
    {
        Collision3DInterop output{};
        output.Colliders = mono_array_new(MonoManager::Get().GetDomain(), ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(),
                                          static_cast<uintptr_t>(collision.Colliders.size()));
        for (size_t i = 0; i < collision.Colliders.size(); ++i)
        {
            ScriptEntity* entity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(collision.Colliders[i]);
            if (entity != nullptr)
                mono_array_setref(output.Colliders, i, entity->GetManagedInstance());
        }

        MonoClass* contactClass = MonoManager::Get().FindClass("Crowny", "ContactPoint3D");
        if (contactClass == nullptr)
            return output;
        output.Contacts =
          mono_array_new(MonoManager::Get().GetDomain(), contactClass->GetInternalPtr(), static_cast<uintptr_t>(collision.Points.size()));
        for (size_t i = 0; i < collision.Points.size(); ++i)
        {
            const PhysicsContactPoint3D& point = collision.Points[i];
            const ContactPoint3DInterop managedPoint{ point.Point, point.Normal, point.Separation, point.NormalImpulse };
            mono_array_set(output.Contacts, ContactPoint3DInterop, i, managedPoint);
        }
        return output;
    }

    using Collision3DThunk = void(CW_THUNKCALL*)(MonoObject*, MonoObject*, MonoException**);
    using Trigger3DThunk = void(CW_THUNKCALL*)(MonoObject*, MonoObject*, MonoException**);

    static void InvokeCollision3D(Collision3DThunk thunk, MonoScript* script, const Collision3D& collision)
    {
        if (thunk == nullptr || script->GetManagedInstance() == nullptr)
            return;
        const Collision3DInterop data = CollisionDataToManaged(collision);
        MonoClass* collisionClass = MonoManager::Get().FindClass("Crowny", "Collision3D");
        if (collisionClass == nullptr)
            return;
        MonoObject* managedCollision = MonoUtils::Box(collisionClass->GetInternalPtr(), (void*)&data);
        MonoUtils::InvokeThunk(thunk, script->GetManagedInstance(), managedCollision);
    }

    void MonoScript::OnCollisionEnter3D(const Collision3D& collision) { InvokeCollision3D(m_OnCollisionEnter3DThunk, this, collision); }

    void MonoScript::OnCollisionStay3D(const Collision3D& collision) { InvokeCollision3D(m_OnCollisionStay3DThunk, this, collision); }

    void MonoScript::OnCollisionExit3D(const Collision3D& collision) { InvokeCollision3D(m_OnCollisionExit3DThunk, this, collision); }

    static void InvokeTrigger3D(Trigger3DThunk thunk, MonoScript* script, Entity other)
    {
        if (thunk == nullptr || script->GetManagedInstance() == nullptr)
            return;
        ScriptEntity* managedOther = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(other);
        if (managedOther != nullptr)
            MonoUtils::InvokeThunk(thunk, script->GetManagedInstance(), managedOther->GetManagedInstance());
    }

    void MonoScript::OnTriggerEnter3D(Entity other) { InvokeTrigger3D(m_OnTriggerEnter3DThunk, this, other); }

    void MonoScript::OnTriggerStay3D(Entity other) { InvokeTrigger3D(m_OnTriggerStay3DThunk, this, other); }

    void MonoScript::OnTriggerExit3D(Entity other) { InvokeTrigger3D(m_OnTriggerExit3DThunk, this, other); }
} // namespace Crowny
