#include "cwpch.h"

#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioMixer.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{

    ScriptAudioMixer::ScriptAudioMixer(MonoObject* instance, const AssetHandle<AudioMixer>& mixer) : TScriptAsset(instance, mixer) {}

    void ScriptAudioMixer::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_SetActive", (void*)&Internal_SetActive);
        MetaData.ScriptClass->AddInternalCall("Internal_GetBusVolume", (void*)&Internal_GetBusVolume);
        MetaData.ScriptClass->AddInternalCall("Internal_SetBusVolume", (void*)&Internal_SetBusVolume);
        MetaData.ScriptClass->AddInternalCall("Internal_IsBusMuted", (void*)&Internal_IsBusMuted);
        MetaData.ScriptClass->AddInternalCall("Internal_SetBusMuted", (void*)&Internal_SetBusMuted);
    }

    void ScriptAudioMixer::Internal_SetActive(ScriptAudioMixer* thisPtr) { gAudioManager->SetActiveMixer(thisPtr->GetHandle()); }

    float ScriptAudioMixer::Internal_GetBusVolume(ScriptAudioMixer* thisPtr, MonoString* name)
    {
        if (!thisPtr->GetHandle() || name == nullptr)
            return 0.0f;
        const String busName = MonoUtils::FromMonoString(name);
        if (Ref<AudioBus> bus = thisPtr->GetHandle()->FindBus(busName))
            return bus->GetVolume();
        return 0.0f;
    }

    void ScriptAudioMixer::Internal_SetBusVolume(ScriptAudioMixer* thisPtr, MonoString* name, float volume)
    {
        if (!thisPtr->GetHandle() || name == nullptr)
            return;
        const String busName = MonoUtils::FromMonoString(name);
        // Update the design-time desc so the change persists if the mixer is saved, then sync.
        for (AudioBusDesc& desc : thisPtr->GetHandle()->GetBusDescs())
        {
            if (desc.Name == busName)
            {
                desc.Volume = volume;
                break;
            }
        }
        thisPtr->GetHandle()->SyncRuntimeFromDescs();
    }

    bool ScriptAudioMixer::Internal_IsBusMuted(ScriptAudioMixer* thisPtr, MonoString* name)
    {
        if (!thisPtr->GetHandle() || name == nullptr)
            return false;
        const String busName = MonoUtils::FromMonoString(name);
        if (Ref<AudioBus> bus = thisPtr->GetHandle()->FindBus(busName))
            return bus->IsMuted();
        return false;
    }

    void ScriptAudioMixer::Internal_SetBusMuted(ScriptAudioMixer* thisPtr, MonoString* name, bool muted)
    {
        if (!thisPtr->GetHandle() || name == nullptr)
            return;
        const String busName = MonoUtils::FromMonoString(name);
        for (AudioBusDesc& desc : thisPtr->GetHandle()->GetBusDescs())
        {
            if (desc.Name == busName)
            {
                desc.Muted = muted;
                break;
            }
        }
        thisPtr->GetHandle()->SyncRuntimeFromDescs();
    }

} // namespace Crowny
