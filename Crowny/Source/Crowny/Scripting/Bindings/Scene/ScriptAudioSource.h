#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{

    class ScriptAudioSource : public TScriptComponent<ScriptAudioSource, AudioSourceComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AudioSource")
        ScriptAudioSource(MonoObject* instance, Entity entity);

    private:
        static void Internal_SetVolume(ScriptAudioSource* thisPtr, float volume);
        static void Internal_SetPitch(ScriptAudioSource* thisPtr, float pitch);
        static void Internal_SetClip(ScriptAudioSource* thisPtr, MonoObject* clip);
        static void Internal_SetPlayOnAwake(ScriptAudioSource* thisPtr, bool playOnAwake);
        static void Internal_SetMinDistance(ScriptAudioSource* thisPtr, float minDistnace);
        static void Internal_SetMaxDistance(ScriptAudioSource* thisPtr, float maxDistance);
        static void Internal_SetLooping(ScriptAudioSource* thisPtr, bool loop);
        static void Internal_SetIsMuted(ScriptAudioSource* thisPtr, bool muted);
        static void Internal_SetTime(ScriptAudioSource* thisPtr, float time);

        static AudioSourceState Internal_GetState(ScriptAudioSource* thisPtr);
        static float Internal_GetVolume(ScriptAudioSource* thisPtr);
        static float Internal_GetPitch(ScriptAudioSource* thisPtr);
        static MonoObject* Internal_GetClip(ScriptAudioSource* thisPtr);
        static bool Internal_GetPlayOnAwake(ScriptAudioSource* thisPtr);
        static float Internal_GetMinDistance(ScriptAudioSource* thisPtr);
        static float Internal_GetMaxDistance(ScriptAudioSource* thisPtr);
        static bool Internal_GetLooping(ScriptAudioSource* thisPtr);
        static bool Internal_GetIsMuted(ScriptAudioSource* thisPtr);
        static float Internal_GetTime(ScriptAudioSource* thisPtr);

        static void Internal_Play(ScriptAudioSource* thisPtr);
        static void Internal_Pause(ScriptAudioSource* thisPtr);
        static void Internal_Stop(ScriptAudioSource* thisPtr);
    };

} // namespace Crowny