#include "cwpch.h"

#include "Crowny/Ecs/Components.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

namespace Crowny
{

    void AudioSourceComponent::OnInitialize()
    {
        if (m_Internal != nullptr)
            return;
        m_Internal = gAudio().CreateSource();
        m_Internal->SetClip(AssetManager::Get().Load<AudioClip>("Resources/Audio/test.asset"));
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

    MonoScriptComponent::MonoScriptComponent(const String& name) { SetClassName(name); }

    MonoClass* MonoScriptComponent::GetManagedClass() const { return m_Class; }
    MonoObject* MonoScriptComponent::GetManagedInstance() const
    {
        return m_ScriptEntityBehaviour->GetManagedInstance();
    }

    void MonoScriptComponent::OnInitialize(Entity entity)
    {
        if (!m_Class)
            return;
        MonoObject* instance = m_Class->CreateInstance();

        m_ScriptEntityBehaviour = static_cast<ScriptEntityBehaviour*>(
          ScriptSceneObjectManager::Get().CreateScriptComponent(instance, entity, *this));

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
    }

    void MonoScriptComponent::SetClassName(const String& className)
    {
        m_Class = MonoManager::Get().GetAssembly(GAME_ASSEMBLY)->GetClass("Sandbox", className);
        if (m_Class != nullptr)
        {
            for (auto* field : m_Class->GetFields()) // TODO: These should not be normal Mono fields
            {
                bool isHidden = field->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().HideInInspector);
                bool isVisible =
                  field->GetVisibility() == CrownyMonoVisibility::Public ||
                  field->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().SerializeFieldAttribute) ||
                  field->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().ShowInInspector);
                if (field != nullptr && !isHidden && isVisible)
                    m_DisplayableFields.push_back(field);
            }

            for (auto* prop : m_Class->GetProperties())
            {
                bool isHidden = prop->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().HideInInspector);
                bool isVisible =
                  prop->GetVisibility() == CrownyMonoVisibility::Public ||
                  prop->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().SerializeFieldAttribute) ||
                  prop->HasAttribute(ScriptInfoManager::Get().GetBuiltinClasses().ShowInInspector);
                if (prop && !isHidden && isVisible)
                    m_DisplayableProperties.push_back(prop);
            }
        }
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