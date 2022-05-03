#include "cwpch.h"

#include <mono/metadata/object.h>

#include "Crowny/Ecs/Components.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Serialization/FileEncoder.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

#include <box2d/box2d.h>

namespace Crowny
{

    RelationshipComponent& RelationshipComponent::operator=(const RelationshipComponent& other)
    {
        Parent = other.Parent;
        return *this;
    }

    void AudioSourceComponent::OnInitialize()
    {
        if (m_Internal == nullptr)
            m_Internal = gAudio().CreateSource();
        m_Internal->SetClip(m_AudioClip);
        m_Internal->SetVolume(m_Volume);
        m_Internal->SetPitch(m_Pitch);
        m_Internal->SetLooping(m_Loop);
        m_Internal->SetMinDistance(m_MinDistance);
        m_Internal->SetMaxDistance(m_MaxDistance);
        m_Internal->SetTime(m_Time);
        m_PlayOnAwake = true;
        if (m_PlayOnAwake)
            m_Internal->Play();
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
        if (m_AudioClip == clip)
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
        if (m_Time != time)
            return;
        m_Time = time;
        if (m_Internal != nullptr)
            m_Internal->SetTime(m_Time);
    }

    void AudioSourceComponent::SetPlayOnAwake(bool playOnAwake) { m_PlayOnAwake = playOnAwake; }

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

    void Rigidbody2DComponent::SetLayerMask(uint32_t layerMask, Entity e)
    {
        if (RuntimeBody != nullptr)
        {
            b2Filter newFilter;
            newFilter.maskBits = 1 << layerMask;
            newFilter.categoryBits = Physics2D::Get().GetCategoryMask(layerMask);
            if (e.HasComponent<BoxCollider2DComponent>())
                e.GetComponent<BoxCollider2DComponent>().RuntimeFixture->SetFilterData(newFilter);
            if (e.HasComponent<CircleCollider2DComponent>())
                e.GetComponent<CircleCollider2DComponent>().RuntimeFixture->SetFilterData(newFilter);
        }
        m_LayerMask = layerMask;
    }

    void Rigidbody2DComponent::SetBodyType(RigidbodyBodyType bodyType)
    {
        if (RuntimeBody != nullptr)
        {
            if (bodyType == RigidbodyBodyType::Static)
                RuntimeBody->SetType(b2_staticBody);
            else if (bodyType == RigidbodyBodyType::Dynamic)
                RuntimeBody->SetType(b2_dynamicBody);
            else if (bodyType == RigidbodyBodyType::Kinematic)
                RuntimeBody->SetType(b2_kinematicBody);
        }
        m_Type = bodyType;
    }

    void Rigidbody2DComponent::SetMass(float mass)
    {
        if (RuntimeBody != nullptr)
        {
            b2MassData massData;
            massData.mass = mass;
            RuntimeBody->SetMassData(&massData);
        }
        m_Mass = mass;
    }

    void Rigidbody2DComponent::SetGravityScale(float scale)
    {
        if (RuntimeBody != nullptr)
            RuntimeBody->SetGravityScale(scale);
        m_GravityScale = scale;
    }

    void Rigidbody2DComponent::SetConstraints(Rigidbody2DConstraints constraints)
    {
        if (RuntimeBody != nullptr)
        {
            if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezeRotation))
                RuntimeBody->SetFixedRotation(true);
            else
                RuntimeBody->SetFixedRotation(false);
        }
        m_Constraints = constraints;
    }

    void Rigidbody2DComponent::SetContinuousCollisionDetection(bool value)
    {
        if (RuntimeBody != nullptr)
            RuntimeBody->SetBullet(value);
        m_ContinuousCollisionDetection = value;
    }

    void Rigidbody2DComponent::SetSleepMode(RigidbodySleepMode sleepMode)
    {
        if (RuntimeBody != nullptr)
        {
            if (sleepMode == RigidbodySleepMode::NeverSleep)
                RuntimeBody->SetSleepingAllowed(false);
            else
                RuntimeBody->SetSleepingAllowed(true);
        }
        m_SleepMode = sleepMode;
    }

    void Rigidbody2DComponent::SetLinearDrag(float linearDrag)
    {
        if (RuntimeBody != nullptr)
            RuntimeBody->SetLinearDamping(linearDrag);
        m_LinearDrag = linearDrag;
    }

    void Rigidbody2DComponent::SetAngularDrag(float angularDrag)
    {
        if (RuntimeBody != nullptr)
            RuntimeBody->SetAngularDamping(angularDrag);
        angularDrag = angularDrag;
    }

    MonoScript::MonoScript(const String& name) : m_TypeName(name)
    {
        SetClassName(name);
    }

    MonoClass* MonoScript::GetManagedClass() const { return m_Class; }
    MonoObject* MonoScript::GetManagedInstance() const
    {
        return m_ScriptEntityBehaviour->GetManagedInstance();
    }

    void MonoScript::OnInitialize(Entity entity)
    {
        MonoObject* managedInstance = nullptr;
        if (m_Class == nullptr)
            return;
        m_ObjectInfo = nullptr;
        if (ScriptInfoManager::Get().GetSerializableObjectInfo(m_Class->GetNamespace(), m_Class->GetName(),
                                                               m_ObjectInfo))
        {
            m_MissingType = false;
            managedInstance = m_ObjectInfo->m_MonoClass->CreateInstance();
            MonoClass* requireClass = ScriptInfoManager::Get().GetBuiltinClasses().RequireComponent;
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
                    uint32_t length = *(int32_t*)MonoUtils::Unbox(lengthObj);
                    MonoProperty* itemProp = listClass->GetProperty("Item");
                    for (uint32_t i = 0; i < length; i++)
                    {
                        MonoReflectionType* reflType = (MonoReflectionType*)itemProp->GetIndexed(components, i);
                        ComponentInfo* componentInfo = ScriptInfoManager::Get().GetComponentInfo(reflType);
                        if (componentInfo != nullptr)
                        {
                            if (!componentInfo->HasCallback(entity))
                                componentInfo->AddCallback(entity);
                        }
                        else
                            CW_ENGINE_WARN(
                              "Could not find component class {0} used in RequireComponent for class {1}: ",
                              MonoUtils::GetReflTypeName(reflType), m_Class->GetFullName());
                    }
                }
            }
        }
        else
        {
            managedInstance = nullptr;
            m_MissingType = true;
            CW_ENGINE_WARN("Missing type");
        }
        // ScriptSceneObjectManager::Get().CreateManagedScriptComponent(managedInstance, component); // TODO: Create a
        // managed component so that in C# land we can do ManagedComponent c = GetComponent<ManagedComponent>(); and not
        // have to cast it

        // m_ScriptEntityBehaviour = static_cast<ScriptEntityBehaviour*>(ScriptSceneObjectManager::Get().CreateScriptComponent(managedInstance, entity, *this));

        if (m_OnStartThunk == nullptr)
        {
            MonoMethod* onStartMethod = m_Class->GetMethod("Start", 0);
            if (onStartMethod != nullptr)
                m_OnStartThunk = (OnStartThunkDef)onStartMethod->GetThunk();
        }

        if (m_OnUpdateThunk == nullptr)
        {
            MonoMethod* onUpdateMethod = m_Class->GetMethod("Update", 0);
            if (onUpdateMethod != nullptr)
                m_OnUpdateThunk = (OnUpdateThunkDef)onUpdateMethod->GetThunk();
        }

        if (m_OnDestroyThunk == nullptr)
        {
            MonoMethod* onDestroyMethod = m_Class->GetMethod("Destroy", 0);
            if (onDestroyMethod != nullptr)
                m_OnDestroyThunk = (OnDestroyThunkDef)onDestroyMethod->GetThunk();
        }

        MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
        if (m_SerializedObjectData != nullptr && !m_MissingType)
        {
            m_SerializedObjectData->Deserialize(instance, m_ObjectInfo);
            m_SerializedObjectData = nullptr;
        }

        struct CollisionDataInterop
        {
            MonoArray* Colliders;
            MonoArray* ContactPoints;
        };

        auto collisionDataToManaged = [&](const Collision2D& collision) {
            CollisionDataInterop output;
            MonoArray* colliders = mono_array_new(MonoManager::Get().GetDomain(),
                                                  ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(), 2);

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
                mono_array_setref(points, i, MonoUtils::Box(vecClass, (void*)&collision.Points[i]));
            output.ContactPoints = points;
            return output;
        };

        auto onCollisionEnterCallback = [&](const Collision2D& collision) {
            CollisionDataInterop data = collisionDataToManaged(collision);
            MonoObject* managedCollision =
              MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            if (m_OnCollisionEnterThunk != nullptr)
                MonoUtils::InvokeThunk(m_OnCollisionEnterThunk, GetManagedInstance(), managedCollision);
        };

        auto onCollisionStayCallback = [&](const Collision2D& collision) {
            CollisionDataInterop data = collisionDataToManaged(collision);
            MonoObject* managedCollision =
              MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            if (m_OnCollisionStayThunk != nullptr)
                MonoUtils::InvokeThunk(m_OnCollisionStayThunk, GetManagedInstance(), managedCollision);
        };

        auto onCollisionExitCallback = [&](const Collision2D& collision) {
            CollisionDataInterop data = collisionDataToManaged(collision);
            MonoObject* managedCollision =
              MonoUtils::Box(MonoManager::Get().FindClass("Crowny", "Collision2D")->GetInternalPtr(), (void*)&data);
            if (m_OnCollisionExitThunk != nullptr)
                MonoUtils::InvokeThunk(m_OnCollisionExitThunk, GetManagedInstance(), managedCollision);
        };

        MonoMethod* onCollisionEnter = GetManagedClass()->GetMethod(
          "OnCollisionEnter2D",
          "Collision2D"); // Maybe replace these with the other Collider, since Box2D works that way
        if (onCollisionEnter != nullptr)
            m_OnCollisionEnterThunk = (OnCollisionEnterThunkDef)onCollisionEnter->GetThunk();
        MonoMethod* onCollisionStay = GetManagedClass()->GetMethod("OnCollisionStay2D", "Collision2D");
        if (onCollisionStay != nullptr)
            m_OnCollisionStayThunk = (OnCollisionStayThunkDef)onCollisionStay->GetThunk();
        MonoMethod* onCollisionExit = GetManagedClass()->GetMethod("OnCollisionExit2D", "Collision2D");
        if (onCollisionExit != nullptr)
            m_OnCollisionExitThunk = (OnCollisionExitThunkDef)onCollisionExit->GetThunk();
        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            entity.GetComponent<BoxCollider2DComponent>().CallbackEnter = onCollisionEnterCallback;
            entity.GetComponent<BoxCollider2DComponent>().CallbackExit = onCollisionExitCallback;
        }
        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            entity.GetComponent<CircleCollider2DComponent>().CallbackEnter = onCollisionEnterCallback;
            entity.GetComponent<CircleCollider2DComponent>().CallbackExit = onCollisionExitCallback;
        }

        // Could add and call an OnAwake method like in Unity here
    }

    ScriptObjectBackupData MonoScript::BeginRefresh()
    {
        MonoObject* instance = GetManagedInstance();
        Ref<SerializableObject> serializableObject = SerializableObject::CreateFromMonoObject(instance);
        if (serializableObject == nullptr)
            return { nullptr, 0 };
        Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();
        BinaryDataStreamOutputArchive archive(stream);
        archive(serializableObject);
        ScriptObjectBackupData backupData;
        backupData.Size = stream->Size();
        backupData.Data = stream->TakeMemory();
        return backupData;
    }

    void MonoScript::EndRefresh(const ScriptObjectBackupData& data)
    {
        MonoObject* instance = GetManagedInstance();
        if (instance != nullptr && data.Data != nullptr)
        {
            // Deserialize
            // ScriptInfoManager::Get().GetSerializableObjectInfo(namespace, typename, objInfo);
        }
    }

    void MonoScript::SetClassName(const String& className)
    {
        m_Class = MonoManager::Get().GetAssembly(GAME_ASSEMBLY)->GetClass("Sandbox", m_TypeName);
        m_OnStartThunk = nullptr;
        m_OnUpdateThunk = nullptr;
        m_OnDestroyThunk = nullptr;
        m_OnCollisionEnterThunk = nullptr;
        m_OnCollisionStayThunk = nullptr;
        m_OnCollisionExitThunk = nullptr;
    }

    void MonoScript::OnStart()
    {
        if (m_OnStartThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnStartThunk, instance);
        }
    }

    void MonoScript::OnUpdate()
    {
        if (m_OnUpdateThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnUpdateThunk, instance);
        }
    }

    void MonoScript::OnDestroy()
    {
        if (m_OnDestroyThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnDestroyThunk, instance);
        }
    }
} // namespace Crowny