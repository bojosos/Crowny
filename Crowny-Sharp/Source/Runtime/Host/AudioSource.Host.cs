#if !CROWNY_MONO
namespace Crowny
{
    public partial class AudioSource
    {
        private AudioClip GetClipBinding() => ManagedRuntimeContext.CreateAsset<AudioClip>(ManagedRuntimeContext.AudioSourceGetClip(EntityId));
        private void SetClipBinding(AudioClip value) => ManagedRuntimeContext.AudioSourceSetClip(EntityId, value?.uuid ?? UUID.Empty);
    }
}
#endif
