using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class AudioMixer : Asset
    {
        public void SetActive()
        {
            Internal_SetActive(m_InternalPtr);
        }

        public float GetBusVolume(string name)
        {
            return Internal_GetBusVolume(m_InternalPtr, name);
        }

        public void SetBusVolume(string name, float volume)
        {
            Internal_SetBusVolume(m_InternalPtr, name, volume);
        }

        public bool IsBusMuted(string name)
        {
            return Internal_IsBusMuted(m_InternalPtr, name);
        }

        public void SetBusMuted(string name, bool muted)
        {
            Internal_SetBusMuted(m_InternalPtr, name, muted);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetActive(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetBusVolume(IntPtr thisPtr, string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetBusVolume(IntPtr thisPtr, string name, float volume);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsBusMuted(IntPtr thisPtr, string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetBusMuted(IntPtr thisPtr, string name, bool muted);
    }
}
