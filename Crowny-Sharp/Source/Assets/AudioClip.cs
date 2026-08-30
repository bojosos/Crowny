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
        public int bitDepth => ManagedRuntimeContext.AudioClipGetBitDepth(uuid);
        public int channels => ManagedRuntimeContext.AudioClipGetChannels(uuid);
        public int frequency => ManagedRuntimeContext.AudioClipGetFrequency(uuid);
        public int samples => ManagedRuntimeContext.AudioClipGetSamples(uuid);
        public float length => ManagedRuntimeContext.AudioClipGetLength(uuid);
        public AudioReadMode readMode => (AudioReadMode)ManagedRuntimeContext.AudioClipGetReadMode(uuid);
        public AudioFormat format => (AudioFormat)ManagedRuntimeContext.AudioClipGetFormat(uuid);
        public bool is3D => ManagedRuntimeContext.AudioClipGetIs3D(uuid);
    }
}
