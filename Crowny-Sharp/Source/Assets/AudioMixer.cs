namespace Crowny
{
    public class AudioMixer : Asset
    {
        public void SetActive()
        {
            ManagedRuntimeContext.AudioMixerSetActive(m_ManagedUuid);
        }

        public float GetBusVolume(string name)
        {
            return ManagedRuntimeContext.AudioMixerGetBusVolume(m_ManagedUuid, name);
        }

        public void SetBusVolume(string name, float volume)
        {
            ManagedRuntimeContext.AudioMixerSetBusVolume(m_ManagedUuid, name, volume);
        }

        public bool IsBusMuted(string name)
        {
            return ManagedRuntimeContext.AudioMixerIsBusMuted(m_ManagedUuid, name);
        }

        public void SetBusMuted(string name, bool muted)
        {
            ManagedRuntimeContext.AudioMixerSetBusMuted(m_ManagedUuid, name, muted);
        }
    }
}
