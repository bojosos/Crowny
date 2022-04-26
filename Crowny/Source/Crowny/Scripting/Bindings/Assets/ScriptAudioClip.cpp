#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"

namespace Crowny
{
    ScriptAudioClip::ScriptAudioClip(MonoObject* instance, const AssetHandle<AudioClip>& clip)
      : TScriptAsset(instance, clip)
    {
    }

    void ScriptAudioClip::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetBitDepth", (void*)&Internal_GetBitDepth);
        MetaData.ScriptClass->AddInternalCall("Internal_GetChannels", (void*)&Internal_GetChannels);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFrequency", (void*)&Internal_GetFrequency);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSamples", (void*)&Internal_GetSamples);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLength", (void*)&Internal_GetLength);
        MetaData.ScriptClass->AddInternalCall("Internal_GetReadMode", (void*)&Internal_GetReadMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetFormat", (void*)&Internal_GetFormat);
        MetaData.ScriptClass->AddInternalCall("Internal_Is3D", (void*)&Internal_Is3D);
    }

    int ScriptAudioClip::Internal_GetBitDepth(ScriptAudioClip* thisPtr)
    {
        return thisPtr->GetHandle()->GetDesc().BitDepth;
    }

    int ScriptAudioClip::Internal_GetChannels(ScriptAudioClip* thisPtr)
    {
        return thisPtr->GetHandle()->GetDesc().NumChannels;
    }

    int ScriptAudioClip::Internal_GetFrequency(ScriptAudioClip* thisPtr)
    {
        return thisPtr->GetHandle()->GetDesc().Frequency;
    }

    int ScriptAudioClip::Internal_GetSamples(ScriptAudioClip* thisPtr) { return thisPtr->GetHandle()->GetNumSamples(); }

    float ScriptAudioClip::Internal_GetLength(ScriptAudioClip* thisPtr) { return thisPtr->GetHandle()->GetLength(); }

    AudioReadMode ScriptAudioClip::Internal_GetReadMode(ScriptAudioClip* thisPtr)
    {
        return thisPtr->GetHandle()->GetDesc().ReadMode;
    }

    AudioFormat ScriptAudioClip::Internal_GetFormat(ScriptAudioClip* thisPtr)
    {
        return thisPtr->GetHandle()->GetDesc().Format;
    }

    bool ScriptAudioClip::Internal_Is3D(ScriptAudioClip* thisPtr) { return thisPtr->GetHandle()->GetDesc().Is3D; }
} // namespace Crowny