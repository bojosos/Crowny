using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public enum AudioSourceState
    {
        Playing,
        Paused,
        Stopped
    };

    public class AudioSource : Component
    {
        public float volume
        {
            get { return Internal_GetVolume(m_InternalPtr); }
            set { Internal_SetVolume(m_InternalPtr, value); }
        }

        public float pitch
        {
            get { return Internal_GetPitch(m_InternalPtr); }
            set { Internal_SetPitch(m_InternalPtr, value); }
        }
        
        public float minDistance
        {
            get { return Internal_GetMinDistance(m_InternalPtr); }
            set { Internal_SetMinDistance(m_InternalPtr, value); }
        }

        public float maxDistance
        {
            get { return Internal_GetMaxDistance(m_InternalPtr); }
            set { Internal_SetMaxDistance(m_InternalPtr, value); }
        }

        public bool loop
        {
            get { return Internal_GetLooping(m_InternalPtr); }
            set { Internal_SetLooping(m_InternalPtr, value); }
        }

        public bool muted
        {
            get { return Internal_GetIsMuted(m_InternalPtr); }
            set { Internal_SetIsMuted(m_InternalPtr, value); }
        }

        public bool playOnAwake
        {
            get { return Internal_GetPlayOnAwake(m_InternalPtr); }
            set { Internal_SetPlayOnAwake(m_InternalPtr, value); }
        }
        
        public float time
        {
            get { return Internal_GetTime(m_InternalPtr); }
            set { Internal_SetTime(m_InternalPtr, value); }
        }

        public AudioClip clip
        {
            get { return Internal_GetClip(m_InternalPtr); }
            set { Internal_SetClip(m_InternalPtr, value); }
        }

        public AudioSourceState state
        {
            get { return Internal_GetState(m_InternalPtr); }
        }

        public void Play()
        {
            Internal_Play(m_InternalPtr);
        }

        public void Pause()
        {
            Internal_Pause(m_InternalPtr);
        }

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