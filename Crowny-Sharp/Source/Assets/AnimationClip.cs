using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>Imported skeletal, morph, or generic animation data.</summary>
    public class AnimationClip : Asset
    {
        /// <summary>Duration of the clip in seconds.</summary>
        public float length => Internal_GetLength(m_InternalPtr);

        /// <summary>Sampling rate stored by the clip.</summary>
        public float sampleRate => Internal_GetSampleRate(m_InternalPtr);

        /// <summary>Whether the clip stores deltas from a reference pose.</summary>
        public bool isAdditive => Internal_GetIsAdditive(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetLength(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetSampleRate(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetIsAdditive(IntPtr thisPtr);
    }
}
