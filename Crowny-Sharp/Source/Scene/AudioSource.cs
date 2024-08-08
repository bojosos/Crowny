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
            get { return Internal_GetVolume(m_InternalPtr); }
            set { Internal_SetVolume(m_InternalPtr, value); }
        }

        /// <summary>
        /// The pitch of the source.
        /// </summary>
        public float pitch
        {
            get { return Internal_GetPitch(m_InternalPtr); }
            set { Internal_SetPitch(m_InternalPtr, value); }
        }

        /// <summary>
        /// Min distance the source can be heard from.
        /// </summary>
        public float minDistance
        {
            get { return Internal_GetMinDistance(m_InternalPtr); }
            set { Internal_SetMinDistance(m_InternalPtr, value); }
        }

        /// <summary>
        /// Max distance the source can be heard from.
        /// </summary>
        public float maxDistance
        {
            get { return Internal_GetMaxDistance(m_InternalPtr); }
            set { Internal_SetMaxDistance(m_InternalPtr, value); }
        }

        /// <summary>
        /// Looping of the source.
        /// </summary>
        public bool loop
        {
            get { return Internal_GetLooping(m_InternalPtr); }
            set { Internal_SetLooping(m_InternalPtr, value); }
        }

        /// <summary>
        /// Mute/Unmute the source. Sets the volume to 0.
        /// </summary>
        public bool muted
        {
            get { return Internal_GetIsMuted(m_InternalPtr); }
            set { Internal_SetIsMuted(m_InternalPtr, value); }
        }

        /// <summary>
        /// If enabled the clip will play every time the object is enabled.
        /// </summary>
        public bool playOnAwake
        {
            get { return Internal_GetPlayOnAwake(m_InternalPtr); }
            set { Internal_SetPlayOnAwake(m_InternalPtr, value); }
        }

        /// <summary>
        /// The current position in the source.
        /// </summary>
        public float time
        {
            get { return Internal_GetTime(m_InternalPtr); }
            set { Internal_SetTime(m_InternalPtr, value); }
        }

        /// <summary>
        /// The clip attached to the source.
        /// </summary>
        public AudioClip clip
        {
            get { return Internal_GetClip(m_InternalPtr); }
            set { Internal_SetClip(m_InternalPtr, value); }
        }

        /// <summary>
        /// The playback state of the source.
        /// </summary>
        public AudioSourceState state
        {
            get { return Internal_GetState(m_InternalPtr); }
        }

        /// <summary>
        /// Plays the source.
        /// </summary>
        public void Play()
        {
            Internal_Play(m_InternalPtr);
        }

        /// <summary>
        /// Pauses the source.
        /// </summary>
        public void Pause()
        {
            Internal_Pause(m_InternalPtr);
        }

        /// <summary>
        /// Stops the source. Resets the playback time to 0.
        /// </summary>
        public void Stop()
        {
            Internal_Stop(m_InternalPtr);
        }

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

    }
}