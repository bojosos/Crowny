#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{

    ScriptAudioSource::ScriptAudioSource(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptAudioSource::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetVolume", (void*)&Internal_GetVolume);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPitch", (void*)&Internal_GetPitch);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMinDistance", (void*)&Internal_GetMinDistance);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMaxDistance", (void*)&Internal_GetMaxDistance);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLooping", (void*)&Internal_GetLooping);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIsMuted", (void*)&Internal_GetIsMuted);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPlayOnAwake", (void*)&Internal_GetPlayOnAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_GetTime", (void*)&Internal_GetTime);
        MetaData.ScriptClass->AddInternalCall("Internal_GetClip", (void*)&Internal_GetClip);
        MetaData.ScriptClass->AddInternalCall("Internal_GetState", (void*)&Internal_GetState);

        MetaData.ScriptClass->AddInternalCall("Internal_SetVolume", (void*)&Internal_SetVolume);
        MetaData.ScriptClass->AddInternalCall("Internal_SetPitch", (void*)&Internal_SetPitch);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMinDistance", (void*)&Internal_SetMinDistance);
        MetaData.ScriptClass->AddInternalCall("Internal_SetMaxDistance", (void*)&Internal_SetMaxDistance);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLooping", (void*)&Internal_SetLooping);
        MetaData.ScriptClass->AddInternalCall("Internal_SetIsMuted", (void*)&Internal_SetIsMuted);
        MetaData.ScriptClass->AddInternalCall("Internal_SetPlayOnAwake", (void*)&Internal_SetPlayOnAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTime", (void*)&Internal_SetTime);
        MetaData.ScriptClass->AddInternalCall("Internal_SetClip", (void*)&Internal_SetClip);

        MetaData.ScriptClass->AddInternalCall("Internal_Play", (void*)&Internal_Play);
        MetaData.ScriptClass->AddInternalCall("Internal_Pause", (void*)&Internal_Pause);
        MetaData.ScriptClass->AddInternalCall("Internal_Stop", (void*)&Internal_Stop);

        MetaData.ScriptClass->AddInternalCall("Internal_GetBus", (void*)&Internal_GetBus);
        MetaData.ScriptClass->AddInternalCall("Internal_SetBus", (void*)&Internal_SetBus);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLowPassGain", (void*)&Internal_GetLowPassGain);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLowPassGain", (void*)&Internal_SetLowPassGain);
        MetaData.ScriptClass->AddInternalCall("Internal_GetHighPassGain", (void*)&Internal_GetHighPassGain);
        MetaData.ScriptClass->AddInternalCall("Internal_SetHighPassGain", (void*)&Internal_SetHighPassGain);

        MetaData.ScriptClass->AddInternalCall("Internal_GetConeInnerAngle", (void*)&Internal_GetConeInnerAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConeInnerAngle", (void*)&Internal_SetConeInnerAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConeOuterAngle", (void*)&Internal_GetConeOuterAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConeOuterAngle", (void*)&Internal_SetConeOuterAngle);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConeOuterGain", (void*)&Internal_GetConeOuterGain);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConeOuterGain", (void*)&Internal_SetConeOuterGain);
        MetaData.ScriptClass->AddInternalCall("Internal_GetConeOuterGainHF", (void*)&Internal_GetConeOuterGainHF);
        MetaData.ScriptClass->AddInternalCall("Internal_SetConeOuterGainHF", (void*)&Internal_SetConeOuterGainHF);
    }

    void ScriptAudioSource::Internal_SetVolume(ScriptAudioSource* thisPtr, float volume) { thisPtr->GetComponent().SetVolume(volume); }

    void ScriptAudioSource::Internal_SetPitch(ScriptAudioSource* thisPtr, float pitch) { thisPtr->GetComponent().SetPitch(pitch); }

    void ScriptAudioSource::Internal_SetClip(ScriptAudioSource* thisPtr, MonoObject* clip)
    {
        ScriptAudioClip* const audioClip = ScriptAudioClip::ToNative(clip);
        if (audioClip != nullptr)
            thisPtr->GetComponent().SetClip(audioClip->GetHandle());
        else
            thisPtr->GetComponent().SetClip(AssetHandle<AudioClip>());
    }

    void ScriptAudioSource::Internal_SetPlayOnAwake(ScriptAudioSource* thisPtr, bool playOnAwake)
    {
        thisPtr->GetComponent().SetPlayOnAwake(playOnAwake);
    }

    void ScriptAudioSource::Internal_SetMinDistance(ScriptAudioSource* thisPtr, float minDistnace)
    {
        thisPtr->GetComponent().SetMinDistance(minDistnace);
    }

    void ScriptAudioSource::Internal_SetMaxDistance(ScriptAudioSource* thisPtr, float maxDistance)
    {
        thisPtr->GetComponent().SetMaxDistance(maxDistance);
    }

    void ScriptAudioSource::Internal_SetLooping(ScriptAudioSource* thisPtr, bool loop) { thisPtr->GetComponent().SetLooping(loop); }

    void ScriptAudioSource::Internal_SetIsMuted(ScriptAudioSource* thisPtr, bool muted) { thisPtr->GetComponent().SetIsMuted(muted); }

    void ScriptAudioSource::Internal_SetTime(ScriptAudioSource* thisPtr, float time) { thisPtr->GetComponent().SetTime(time); }

    AudioSourceState ScriptAudioSource::Internal_GetState(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetState(); }

    float ScriptAudioSource::Internal_GetVolume(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetVolume(); }

    float ScriptAudioSource::Internal_GetPitch(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetPitch(); }

    MonoObject* ScriptAudioSource::Internal_GetClip(ScriptAudioSource* thisPtr)
    {
        ScriptAssetBase* const asset = ScriptAssetManager::Get().GetScriptAsset(thisPtr->GetComponent().GetClip(), true);
        if (asset != nullptr)
            return asset->GetManagedInstance();
        return nullptr;
    }

    bool ScriptAudioSource::Internal_GetPlayOnAwake(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetPlayOnAwake(); }

    float ScriptAudioSource::Internal_GetMinDistance(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetMinDistance(); }

    float ScriptAudioSource::Internal_GetMaxDistance(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetMaxDistance(); }

    bool ScriptAudioSource::Internal_GetLooping(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetLooping(); }

    bool ScriptAudioSource::Internal_GetIsMuted(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetIsMuted(); }

    float ScriptAudioSource::Internal_GetTime(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetTime(); }

    void ScriptAudioSource::Internal_Play(ScriptAudioSource* thisPtr) { thisPtr->GetComponent().Play(); }

    void ScriptAudioSource::Internal_Pause(ScriptAudioSource* thisPtr) { thisPtr->GetComponent().Pause(); }

    void ScriptAudioSource::Internal_Stop(ScriptAudioSource* thisPtr) { thisPtr->GetComponent().Stop(); }

    MonoString* ScriptAudioSource::Internal_GetBus(ScriptAudioSource* thisPtr)
    {
        return MonoUtils::ToMonoString(thisPtr->GetComponent().GetBusName());
    }

    void ScriptAudioSource::Internal_SetBus(ScriptAudioSource* thisPtr, MonoString* bus)
    {
        thisPtr->GetComponent().SetBusName(bus != nullptr ? MonoUtils::FromMonoString(bus) : String());
    }

    float ScriptAudioSource::Internal_GetLowPassGain(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetLowPassGain(); }

    void ScriptAudioSource::Internal_SetLowPassGain(ScriptAudioSource* thisPtr, float gainHF)
    {
        thisPtr->GetComponent().SetLowPassGain(gainHF);
    }

    float ScriptAudioSource::Internal_GetHighPassGain(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetHighPassGain(); }

    void ScriptAudioSource::Internal_SetHighPassGain(ScriptAudioSource* thisPtr, float gainLF)
    {
        thisPtr->GetComponent().SetHighPassGain(gainLF);
    }

    float ScriptAudioSource::Internal_GetConeInnerAngle(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetConeInnerAngle(); }

    void ScriptAudioSource::Internal_SetConeInnerAngle(ScriptAudioSource* thisPtr, float degrees)
    {
        thisPtr->GetComponent().SetConeInnerAngle(degrees);
    }

    float ScriptAudioSource::Internal_GetConeOuterAngle(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetConeOuterAngle(); }

    void ScriptAudioSource::Internal_SetConeOuterAngle(ScriptAudioSource* thisPtr, float degrees)
    {
        thisPtr->GetComponent().SetConeOuterAngle(degrees);
    }

    float ScriptAudioSource::Internal_GetConeOuterGain(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetConeOuterGain(); }

    void ScriptAudioSource::Internal_SetConeOuterGain(ScriptAudioSource* thisPtr, float gain)
    {
        thisPtr->GetComponent().SetConeOuterGain(gain);
    }

    float ScriptAudioSource::Internal_GetConeOuterGainHF(ScriptAudioSource* thisPtr) { return thisPtr->GetComponent().GetConeOuterGainHF(); }

    void ScriptAudioSource::Internal_SetConeOuterGainHF(ScriptAudioSource* thisPtr, float gainHF)
    {
        thisPtr->GetComponent().SetConeOuterGainHF(gainHF);
    }
} // namespace Crowny