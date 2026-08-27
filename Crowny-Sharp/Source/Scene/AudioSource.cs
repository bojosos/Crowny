using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>
    /// Audio source playback state.
    /// </summary>
    public enum AudioSourceState
    {
        Playing,
        Paused,
        Stopped
    };

    public class AudioSource : Component
    {
        /// <summary>
        /// The volume of the source.
        /// </summary>
        /// <value>A value in the range [0.0, 1.0].</value>
        public float volume
        {
            get { return GetVolume(); }
            set { SetVolume(value); }
        }

        /// <summary>
        /// The pitch of the source.
        /// </summary>
        public float pitch
        {
            get { return GetPitch(); }
            set { SetPitch(value); }
        }

        /// <summary>
        /// Min distance the source can be heard from.
        /// </summary>
        public float minDistance
        {
            get { return GetMinDistance(); }
            set { SetMinDistance(value); }
        }

        /// <summary>
        /// Max distance the source can be heard from.
        /// </summary>
        public float maxDistance
        {
            get { return GetMaxDistance(); }
            set { SetMaxDistance(value); }
        }

        /// <summary>
        /// Looping of the source.
        /// </summary>
        public bool loop
        {
            get { return GetLooping(); }
            set { SetLooping(value); }
        }

        /// <summary>
        /// Mute/Unmute the source. Sets the volume to 0.
        /// </summary>
        public bool muted
        {
            get { return GetIsMuted(); }
            set { SetIsMuted(value); }
        }

        /// <summary>
        /// If enabled the clip will play every time the object is enabled.
        /// </summary>
        public bool playOnAwake
        {
            get { return GetPlayOnAwake(); }
            set { SetPlayOnAwake(value); }
        }

        /// <summary>
        /// The current position in the source.
        /// </summary>
        public float time
        {
            get { return GetTime(); }
            set { SetTime(value); }
        }

        /// <summary>
        /// The clip attached to the source.
        /// </summary>
        public AudioClip clip
        {
            get { return GetClip(); }
            set { SetClip(value); }
        }

        /// <summary>
        /// The playback state of the source.
        /// </summary>
        public AudioSourceState state
        {
            get { return GetState(); }
        }

        /// <summary>
        /// Plays the source.
        /// </summary>
        public void Play()
        {
            InvokePlayback(ManagedBindingId.AudioSourcePlay);
        }

        /// <summary>
        /// Pauses the source.
        /// </summary>
        public void Pause()
        {
            InvokePlayback(ManagedBindingId.AudioSourcePause);
        }

        /// <summary>
        /// Stops the source. Resets the playback time to 0.
        /// </summary>
        public void Stop()
        {
            InvokePlayback(ManagedBindingId.AudioSourceStop);
        }

#if CROWNY_MONO
        private float GetVolume() => Internal_GetVolume(m_InternalPtr);
        private void SetVolume(float value) => Internal_SetVolume(m_InternalPtr, value);
        private float GetPitch() => Internal_GetPitch(m_InternalPtr);
        private void SetPitch(float value) => Internal_SetPitch(m_InternalPtr, value);
        private float GetMinDistance() => Internal_GetMinDistance(m_InternalPtr);
        private void SetMinDistance(float value) => Internal_SetMinDistance(m_InternalPtr, value);
        private float GetMaxDistance() => Internal_GetMaxDistance(m_InternalPtr);
        private void SetMaxDistance(float value) => Internal_SetMaxDistance(m_InternalPtr, value);
        private bool GetLooping() => Internal_GetLooping(m_InternalPtr);
        private void SetLooping(bool value) => Internal_SetLooping(m_InternalPtr, value);
        private bool GetIsMuted() => Internal_GetIsMuted(m_InternalPtr);
        private void SetIsMuted(bool value) => Internal_SetIsMuted(m_InternalPtr, value);
        private bool GetPlayOnAwake() => Internal_GetPlayOnAwake(m_InternalPtr);
        private void SetPlayOnAwake(bool value) => Internal_SetPlayOnAwake(m_InternalPtr, value);
        private float GetTime() => Internal_GetTime(m_InternalPtr);
        private void SetTime(float value) => Internal_SetTime(m_InternalPtr, value);
        private AudioClip GetClip() => Internal_GetClip(m_InternalPtr);
        private void SetClip(AudioClip value) => Internal_SetClip(m_InternalPtr, value);
        private AudioSourceState GetState() => Internal_GetState(m_InternalPtr);
        private void InvokePlayback(ManagedBindingId binding)
        {
            if (binding == ManagedBindingId.AudioSourcePlay)
                Internal_Play(m_InternalPtr);
            else if (binding == ManagedBindingId.AudioSourcePause)
                Internal_Pause(m_InternalPtr);
            else
                Internal_Stop(m_InternalPtr);
        }
#else
        private UUID EntityId => entity.uuid;
        private float GetVolume() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.AudioSourceGetVolume, EntityId);
        private void SetVolume(float value) => ManagedRuntimeContext.SetBindingFloat(ManagedBindingId.AudioSourceSetVolume, EntityId, value);
        private float GetPitch() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.AudioSourceGetPitch, EntityId);
        private void SetPitch(float value) => ManagedRuntimeContext.SetBindingFloat(ManagedBindingId.AudioSourceSetPitch, EntityId, value);
        private float GetMinDistance() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.AudioSourceGetMinDistance, EntityId);
        private void SetMinDistance(float value) => ManagedRuntimeContext.SetBindingFloat(ManagedBindingId.AudioSourceSetMinDistance, EntityId, value);
        private float GetMaxDistance() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.AudioSourceGetMaxDistance, EntityId);
        private void SetMaxDistance(float value) => ManagedRuntimeContext.SetBindingFloat(ManagedBindingId.AudioSourceSetMaxDistance, EntityId, value);
        private bool GetLooping() => ManagedRuntimeContext.GetBindingBoolean(ManagedBindingId.AudioSourceGetLoop, EntityId);
        private void SetLooping(bool value) => ManagedRuntimeContext.SetBindingBoolean(ManagedBindingId.AudioSourceSetLoop, EntityId, value);
        private bool GetIsMuted() => ManagedRuntimeContext.GetBindingBoolean(ManagedBindingId.AudioSourceGetMuted, EntityId);
        private void SetIsMuted(bool value) => ManagedRuntimeContext.SetBindingBoolean(ManagedBindingId.AudioSourceSetMuted, EntityId, value);
        private bool GetPlayOnAwake() => ManagedRuntimeContext.GetBindingBoolean(ManagedBindingId.AudioSourceGetPlayOnAwake, EntityId);
        private void SetPlayOnAwake(bool value) => ManagedRuntimeContext.SetBindingBoolean(ManagedBindingId.AudioSourceSetPlayOnAwake, EntityId, value);
        private float GetTime() => ManagedRuntimeContext.GetBindingFloat(ManagedBindingId.AudioSourceGetTime, EntityId);
        private void SetTime(float value) => ManagedRuntimeContext.SetBindingFloat(ManagedBindingId.AudioSourceSetTime, EntityId, value);
        private AudioClip GetClip() => ManagedRuntimeContext.CreateAsset<AudioClip>(ManagedRuntimeContext.GetBindingUuid(ManagedBindingId.AudioSourceGetClip, EntityId));
        private void SetClip(AudioClip value) => ManagedRuntimeContext.SetBindingUuid(ManagedBindingId.AudioSourceSetClip, EntityId,
                                                                                       value?.uuid ?? UUID.Empty);
        private AudioSourceState GetState() =>
            (AudioSourceState)ManagedRuntimeContext.GetBindingInt32(ManagedBindingId.AudioSourceGetState, EntityId);
        private void InvokePlayback(ManagedBindingId binding) => ManagedRuntimeContext.InvokeBinding(binding, EntityId);
#endif

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)]
		private static extern float Internal_GetVolume(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetVolume(IntPtr parent, float volume);
        [MethodImpl(MethodImplOptions.InternalCall)]
		private static extern float Internal_GetPitch(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetPitch(IntPtr parent, float pitch);
        [MethodImpl(MethodImplOptions.InternalCall)]
		private static extern float Internal_GetMinDistance(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMinDistance(IntPtr parent, float distance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetMaxDistance(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMaxDistance(IntPtr parent, float distance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetLooping(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLooping(IntPtr parent, bool loop);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetIsMuted(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetIsMuted(IntPtr parent, bool muted);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetPlayOnAwake(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetPlayOnAwake(IntPtr parent, bool playOnAwake);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetTime(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTime(IntPtr parent, float time);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AudioClip Internal_GetClip(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetClip(IntPtr parent, AudioClip clip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AudioSourceState Internal_GetState(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Play(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Pause(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Stop(IntPtr parent);
#endif

    }
}
