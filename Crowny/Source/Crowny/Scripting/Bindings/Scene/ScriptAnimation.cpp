#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAnimationClip.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAnimation.h"
#include "Crowny/Scripting/ScriptAssetManager.h"

namespace Crowny
{
    ScriptAnimation::ScriptAnimation(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptAnimation::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetClip", (void*)&Internal_GetClip);
        MetaData.ScriptClass->AddInternalCall("Internal_SetClip", (void*)&Internal_SetClip);
        MetaData.ScriptClass->AddInternalCall("Internal_GetSpeed", (void*)&Internal_GetSpeed);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSpeed", (void*)&Internal_SetSpeed);
        MetaData.ScriptClass->AddInternalCall("Internal_GetWrapMode", (void*)&Internal_GetWrapMode);
        MetaData.ScriptClass->AddInternalCall("Internal_SetWrapMode", (void*)&Internal_SetWrapMode);
        MetaData.ScriptClass->AddInternalCall("Internal_GetPlayOnAwake", (void*)&Internal_GetPlayOnAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_SetPlayOnAwake", (void*)&Internal_SetPlayOnAwake);
        MetaData.ScriptClass->AddInternalCall("Internal_GetApplyRootMotion", (void*)&Internal_GetApplyRootMotion);
        MetaData.ScriptClass->AddInternalCall("Internal_SetApplyRootMotion", (void*)&Internal_SetApplyRootMotion);
        MetaData.ScriptClass->AddInternalCall("Internal_GetTime", (void*)&Internal_GetTime);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTime", (void*)&Internal_SetTime);
        MetaData.ScriptClass->AddInternalCall("Internal_GetNormalizedTime", (void*)&Internal_GetNormalizedTime);
        MetaData.ScriptClass->AddInternalCall("Internal_SetNormalizedTime", (void*)&Internal_SetNormalizedTime);
        MetaData.ScriptClass->AddInternalCall("Internal_GetState", (void*)&Internal_GetState);
        MetaData.ScriptClass->AddInternalCall("Internal_Play", (void*)&Internal_Play);
        MetaData.ScriptClass->AddInternalCall("Internal_Pause", (void*)&Internal_Pause);
        MetaData.ScriptClass->AddInternalCall("Internal_Stop", (void*)&Internal_Stop);
    }

    MonoObject* ScriptAnimation::Internal_GetClip(ScriptAnimation* thisPtr)
    {
        if (thisPtr == nullptr || !ScriptAssetManager::IsStartedUp())
            return nullptr;
        const AssetHandle<AnimationClip>& clip = thisPtr->GetComponent().GetClip();
        if (!clip)
            return nullptr;
        ScriptAssetBase* asset = ScriptAssetManager::Get().GetScriptAsset(clip, true);
        return asset != nullptr ? asset->GetManagedInstance() : nullptr;
    }

    void ScriptAnimation::Internal_SetClip(ScriptAnimation* thisPtr, MonoObject* clip)
    {
        if (thisPtr == nullptr)
            return;
        ScriptAnimationClip* nativeClip = ScriptAnimationClip::ToNative(clip);
        thisPtr->GetComponent().SetClip(nativeClip != nullptr ? nativeClip->GetHandle() : AssetHandle<AnimationClip>());
    }

    float ScriptAnimation::Internal_GetSpeed(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr ? thisPtr->GetComponent().GetSpeed() : 1.0f;
    }

    void ScriptAnimation::Internal_SetSpeed(ScriptAnimation* thisPtr, float speed)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetSpeed(speed);
    }

    int32_t ScriptAnimation::Internal_GetWrapMode(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr ? static_cast<int32_t>(thisPtr->GetComponent().GetWrapMode())
                                  : static_cast<int32_t>(AnimationWrapMode::Loop);
    }

    void ScriptAnimation::Internal_SetWrapMode(ScriptAnimation* thisPtr, int32_t wrapMode)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetWrapMode(static_cast<AnimationWrapMode>(wrapMode));
    }

    bool ScriptAnimation::Internal_GetPlayOnAwake(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetComponent().GetPlayOnAwake();
    }

    void ScriptAnimation::Internal_SetPlayOnAwake(ScriptAnimation* thisPtr, bool playOnAwake)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetPlayOnAwake(playOnAwake);
    }

    bool ScriptAnimation::Internal_GetApplyRootMotion(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr && thisPtr->GetComponent().GetApplyRootMotion();
    }

    void ScriptAnimation::Internal_SetApplyRootMotion(ScriptAnimation* thisPtr, bool applyRootMotion)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetApplyRootMotion(applyRootMotion);
    }

    float ScriptAnimation::Internal_GetTime(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr ? thisPtr->GetComponent().GetTime() : 0.0f;
    }

    void ScriptAnimation::Internal_SetTime(ScriptAnimation* thisPtr, float time)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetTime(time);
    }

    float ScriptAnimation::Internal_GetNormalizedTime(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr ? thisPtr->GetComponent().GetNormalizedTime() : 0.0f;
    }

    void ScriptAnimation::Internal_SetNormalizedTime(ScriptAnimation* thisPtr, float normalizedTime)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().SetNormalizedTime(normalizedTime);
    }

    int32_t ScriptAnimation::Internal_GetState(ScriptAnimation* thisPtr)
    {
        return thisPtr != nullptr ? static_cast<int32_t>(thisPtr->GetComponent().GetState())
                                  : static_cast<int32_t>(AnimationPlaybackState::Stopped);
    }

    void ScriptAnimation::Internal_Play(ScriptAnimation* thisPtr)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().Play();
    }

    void ScriptAnimation::Internal_Pause(ScriptAnimation* thisPtr)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().Pause();
    }

    void ScriptAnimation::Internal_Stop(ScriptAnimation* thisPtr)
    {
        if (thisPtr != nullptr)
            thisPtr->GetComponent().Stop();
    }
} // namespace Crowny
