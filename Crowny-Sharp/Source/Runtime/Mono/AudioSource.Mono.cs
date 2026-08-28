#if CROWNY_MONO
using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public partial class AudioSource
    {
        private AudioClip GetClipBinding() => Internal_GetClip(m_InternalPtr);
        private void SetClipBinding(AudioClip value) => Internal_SetClip(m_InternalPtr, value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern AudioClip Internal_GetClip(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetClip(IntPtr parent, AudioClip clip);
    }
}
#endif
