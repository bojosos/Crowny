using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>Behavior used when playback crosses a clip boundary.</summary>
    public enum AnimationWrapMode
    {
        Clamp = 0,
        Loop = 1,
        PingPong = 2
    }

    /// <summary>Current state of an animation component's player.</summary>
    public enum AnimationPlaybackState
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2
    }

    /// <summary>Controls animation playback and deformation for an entity's mesh.</summary>
    public class AnimationComponent : Component
    {
        /// <summary>Clip evaluated by this component.</summary>
        public AnimationClip clip
        {
            get { return Internal_GetClip(m_InternalPtr); }
            set { Internal_SetClip(m_InternalPtr, value); }
        }

        /// <summary>Playback rate multiplier. Negative values play in reverse.</summary>
        public float speed
        {
            get { return Internal_GetSpeed(m_InternalPtr); }
            set { Internal_SetSpeed(m_InternalPtr, value); }
        }

        /// <summary>Behavior used when playback crosses a clip boundary.</summary>
        public AnimationWrapMode wrapMode
        {
            get { return Internal_GetWrapMode(m_InternalPtr); }
            set { Internal_SetWrapMode(m_InternalPtr, value); }
        }

        /// <summary>Whether a new runtime player begins in the playing state.</summary>
        public bool playOnAwake
        {
            get { return Internal_GetPlayOnAwake(m_InternalPtr); }
            set { Internal_SetPlayOnAwake(m_InternalPtr, value); }
        }

        /// <summary>Whether evaluated root-motion deltas are applied to the entity transform.</summary>
        public bool applyRootMotion
        {
            get { return Internal_GetApplyRootMotion(m_InternalPtr); }
            set { Internal_SetApplyRootMotion(m_InternalPtr, value); }
        }

        /// <summary>Current raw playback time in seconds.</summary>
        public float time
        {
            get { return Internal_GetTime(m_InternalPtr); }
            set { Internal_SetTime(m_InternalPtr, value); }
        }

        /// <summary>Raw playback time divided by clip length. Looping values are not clamped.</summary>
        public float normalizedTime
        {
            get { return Internal_GetNormalizedTime(m_InternalPtr); }
            set { Internal_SetNormalizedTime(m_InternalPtr, value); }
        }

        /// <summary>Current playback state.</summary>
        public AnimationPlaybackState state => Internal_GetState(m_InternalPtr);

        /// <summary>Starts the assigned clip from the beginning.</summary>
        public void Play() { Internal_Play(m_InternalPtr); }

        /// <summary>Pauses playback without changing its time.</summary>
        public void Pause() { Internal_Pause(m_InternalPtr); }

        /// <summary>Stops playback and resets its time.</summary>
        public void Stop() { Internal_Stop(m_InternalPtr); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AnimationClip Internal_GetClip(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetClip(IntPtr thisPtr, AnimationClip clip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetSpeed(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSpeed(IntPtr thisPtr, float speed);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AnimationWrapMode Internal_GetWrapMode(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetWrapMode(IntPtr thisPtr, AnimationWrapMode wrapMode);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetPlayOnAwake(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetPlayOnAwake(IntPtr thisPtr, bool playOnAwake);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetApplyRootMotion(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetApplyRootMotion(IntPtr thisPtr, bool applyRootMotion);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetTime(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTime(IntPtr thisPtr, float time);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetNormalizedTime(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetNormalizedTime(IntPtr thisPtr, float normalizedTime);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AnimationPlaybackState Internal_GetState(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Play(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Pause(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Stop(IntPtr thisPtr);
    }
}
