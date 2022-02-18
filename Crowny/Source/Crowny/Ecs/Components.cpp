#include "cwpch.h"

#include "Crowny/Ecs/Components.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

namespace Crowny
{

    RelationshipComponent& RelationshipComponent::operator=(const RelationshipComponent& other)
    {
        Parent = other.Parent;
        return *this;
    }

    void AudioSourceComponent::OnInitialize()
    {
        if (m_Internal != nullptr)
            return;
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

    void AudioSourceComponent::SetClip(const Ref<AudioClip>& clip)
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

    MonoScriptComponent::MonoScriptComponent(const String& name) : ComponentBase() { SetClassName(name); }

    MonoClass* MonoScriptComponent::GetManagedClass() const { return m_Class; }
    MonoObject* MonoScriptComponent::GetManagedInstance() const
    {
        return m_ScriptEntityBehaviour->GetManagedInstance();
    }

    void MonoScriptComponent::OnInitialize(Entity entity)
    {
        MonoObject* managedInstance;
        if (m_Class != nullptr)
        {
            m_ObjectInfo = nullptr;
            if (ScriptInfoManager::Get().GetSerializableObjectInfo(m_Namespace, m_TypeName, m_ObjectInfo))
            {
                m_MissingType = false;
                managedInstance = m_ObjectInfo->m_MonoClass->CreateInstance();
            }
            else
            {
                managedInstance = nullptr;
                m_MissingType = true;
            }
        }
        // ScriptSceneObjectManager::Get().CreateManagedScriptComponent(managedInstance, component); // TODO: Create a managed component so that in C# land 
                                                                                              // we can do ManagedComponent c = GetComponent<ManagedComponent>();
                                                                                              // and not have to cast it

        m_ScriptEntityBehaviour = static_cast<ScriptEntityBehaviour*>(
          ScriptSceneObjectManager::Get().CreateScriptComponent(managedInstance, entity, *this));

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
        // Could add and call an OnAwake method like in Unity here
    }

    ScriptObjectBackupData MonoScriptComponent::BeginRefresh()
    {
        MonoObject* instance = GetManagedInstance();
        Ref<SerializableObject> serializableObject = SerializableObject::CreateFromMonoObject(instance);
        if (serializableObject == nullptr)
            return { nullptr, 0 };
        Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();
        // YamlSerializer ys;
        // ys.Serialize(serializableObject, stream);
        ScriptObjectBackupData backupData;
        // backupData.Size = stream->GetSize();
        // backupData.Data = stream->DisownMemory(); // TODO: implement in datastream
        return backupData;
    }

    void MonoScriptComponent::EndRefresh(const ScriptObjectBackupData& data)
    {
        MonoObject* instance = GetManagedInstance();
        if (instance != nullptr && data.Data != nullptr)
        {
            // Deserialize
            // ScriptInfoManager::Get().GetSerializableObjectInfo(namespace, typename, objInfo);

        }
    }

    void MonoScriptComponent::SetClassName(const String& className)
    {
        m_Class = MonoManager::Get().GetAssembly(GAME_ASSEMBLY)->GetClass("Sandbox", className);
        
    }

    void MonoScriptComponent::OnStart()
    {
        if (m_OnStartThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnStartThunk, instance);
        }
    }

    void MonoScriptComponent::OnUpdate()
    {
        if (m_OnUpdateThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnUpdateThunk, instance);
        }
    }

    void MonoScriptComponent::OnDestroy()
    {
        if (m_OnDestroyThunk != nullptr)
        {
            MonoObject* instance = m_ScriptEntityBehaviour->GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnDestroyThunk, instance);
        }
    }

} // namespace Crowny