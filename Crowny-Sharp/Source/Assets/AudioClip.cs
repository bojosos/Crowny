using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public enum AudioReadMode
    {
        LoadDecompressed = 0,
        LoadCompressed = 1,
        Stream = 2
    }

    public enum AudioFormat
    {
        PCM = 0,
        VORBIS = 1
    }

    public class AudioClip : Asset
    {
        public int bitDepth => Internal_GetBitDepth(m_InternalPtr);
        public int channels => Internal_GetChannels(m_InternalPtr);
        public int frequency => Internal_GetFrequency(m_InternalPtr);
        public int samples => Internal_GetSamples(m_InternalPtr);
        public float length => Internal_GetLength(m_InternalPtr);
        public AudioReadMode readMode => Internal_GetReadMode(m_InternalPtr);
        public AudioFormat format => Internal_GetFormat(m_InternalPtr);
        public bool is3D => Internal_Is3D(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetBitDepth(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetChannels(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetFrequency(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetSamples(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetLength(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AudioReadMode Internal_GetReadMode(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AudioFormat Internal_GetFormat(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_Is3D(IntPtr thisPtr);
    }
}