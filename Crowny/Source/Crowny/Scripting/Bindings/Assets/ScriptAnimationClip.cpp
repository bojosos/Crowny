#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAnimationClip.h"

namespace Crowny
{
    ScriptAnimationClip::ScriptAnimationClip(MonoObject* instance, const AssetHandle<AnimationClip>& clip) : TScriptAsset(instance, clip) {}

    void ScriptAnimationClip::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetLength", (void*)&Internal_GetLength);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSampleRate", (void*)&Internal_GetSampleRate);
        MetaData.ScriptClass->AddInternalCall("Internal_GetIsAdditive", (void*)&Internal_GetIsAdditive);
    }

    float ScriptAnimationClip::Internal_GetLength(ScriptAnimationClip* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetHandle() ? thisPtr->GetHandle()->GetLength() : 0.0f;
    }

    float ScriptAnimationClip::Internal_GetSampleRate(ScriptAnimationClip* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetHandle() ? thisPtr->GetHandle()->GetSampleRate() : 0.0f;
    }

    bool ScriptAnimationClip::Internal_GetIsAdditive(ScriptAnimationClip* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetHandle() && thisPtr->GetHandle()->IsAdditive();
    }
} // namespace Crowny
