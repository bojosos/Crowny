#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptAnimation : public TScriptComponent<ScriptAnimation, AnimationComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AnimationComponent");
        ScriptAnimation(MonoObject* instance, Entity entity);

    private:
        static MonoObject* Internal_GetClip(ScriptAnimation* thisPtr);
        static void Internal_SetClip(ScriptAnimation* thisPtr, MonoObject* clip);
        static float Internal_GetSpeed(ScriptAnimation* thisPtr);
        static void Internal_SetSpeed(ScriptAnimation* thisPtr, float speed);
        static int32_t Internal_GetWrapMode(ScriptAnimation* thisPtr);
        static void Internal_SetWrapMode(ScriptAnimation* thisPtr, int32_t wrapMode);
        static bool Internal_GetPlayOnAwake(ScriptAnimation* thisPtr);
        static void Internal_SetPlayOnAwake(ScriptAnimation* thisPtr, bool playOnAwake);
        static bool Internal_GetApplyRootMotion(ScriptAnimation* thisPtr);
        static void Internal_SetApplyRootMotion(ScriptAnimation* thisPtr, bool applyRootMotion);
        static float Internal_GetTime(ScriptAnimation* thisPtr);
        static void Internal_SetTime(ScriptAnimation* thisPtr, float time);
        static float Internal_GetNormalizedTime(ScriptAnimation* thisPtr);
        static void Internal_SetNormalizedTime(ScriptAnimation* thisPtr, float normalizedTime);
        static int32_t Internal_GetState(ScriptAnimation* thisPtr);
        static void Internal_Play(ScriptAnimation* thisPtr);
        static void Internal_Pause(ScriptAnimation* thisPtr);
        static void Internal_Stop(ScriptAnimation* thisPtr);
    };
} // namespace Crowny
