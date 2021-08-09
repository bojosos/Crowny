#include "cwpch.h"

#include "Crowny/Ecs/Components.h"

namespace Crowny
{
    
    void AudioSourceComponent::Initialize()
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
            m_Internal->SetLooping(loop);
    }

    void AudioSourceComponent::SetPlayOnAwake(bool playOnAwake)
    {
        m_PlayOnAwake = playOnAwake;
    }
    
    void MonoScriptComponent::Initialize()
    {
        if (!m_Class)
            return;

        CWMonoField* scriptPtr = m_Class->GetField("m_InternalPtr");
        MonoObject* scriptInstance = m_Class->CreateInstance();
        m_Handle = MonoUtils::NewGCHandle(scriptInstance, false);
        size_t tmp = (size_t)this;
        scriptPtr->Set(scriptInstance, &tmp);

        CWMonoMethod* ctor = m_Class->GetMethod(".ctor", 0);
        if (ctor)
            ctor->Invoke(scriptInstance, nullptr);

        if (m_OnStartThunk == nullptr)
        {
            CWMonoMethod* onStartMethod = m_Class->GetMethod("OnStart", 0);
            if (onStartMethod != nullptr)
                m_OnStartThunk = (OnStartThunkDef)onStartMethod->GetThunk();
        }

        if (m_OnUpdateThunk == nullptr)
        {
            CWMonoMethod* onUpdateMethod = m_Class->GetMethod("OnUpdate", 0);
            if (onUpdateMethod != nullptr)
                m_OnUpdateThunk = (OnUpdateThunkDef)onUpdateMethod->GetThunk();
        }

        if (m_OnDestroyThunk == nullptr)
        {
            CWMonoMethod* onDestroyMethod = m_Class->GetMethod("OnDestroy", 0);
            if (onDestroyMethod != nullptr)
                m_OnDestroyThunk = (OnDestroyThunkDef)onDestroyMethod->GetThunk();
        }
    }

    MonoScriptComponent::MonoScriptComponent(const std::string& name)
    {
        SetClassName(name);
    }
    
    void MonoScriptComponent::SetClassName(const std::string& className)
    {
        m_Class = CWMonoRuntime::GetClientAssembly()->GetClass("Sandbox", className);
        if (m_Class != nullptr)
        {
            for (auto* field : m_Class->GetFields())
            {
                bool isHidden = field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector);
                bool isVisible = field->GetVisibility() == CWMonoVisibility::Public ||
                                    field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute) ||
                                    field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector);
                if (field != nullptr && !isHidden && isVisible)
                    m_DisplayableFields.push_back(field);
            }

            for (auto* prop : m_Class->GetProperties())
            {
                bool isHidden = prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector);
                bool isVisible = prop->GetVisibility() == CWMonoVisibility::Public ||
                                    prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute) ||
                                    prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector);
                if (prop && !isHidden && isVisible)
                    m_DisplayableProperties.push_back(prop);
            }
        }
    }

    void MonoScriptComponent::OnStart()
    {
        if (m_OnStartThunk != nullptr)
        {
            MonoObject* instance = GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnStartThunk, instance);
        }
    }

    void MonoScriptComponent::OnUpdate()
    {
        if (m_OnUpdateThunk != nullptr)
        {
            MonoObject* instance = GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnUpdateThunk, instance);
        }
    }

    void MonoScriptComponent::OnDestroy()
    {
        if (m_OnDestroyThunk != nullptr)
        {
            MonoObject* instance = GetManagedInstance();
            MonoUtils::InvokeThunk(m_OnDestroyThunk, instance);
        }
    }
    
}