namespace Crowny
{
    /// <summary>Imported skeletal, morph, or generic animation data.</summary>
    public class AnimationClip : Asset
    {
        /// <summary>Duration of the clip in seconds.</summary>
        public float length => ManagedRuntimeContext.AnimationClipGetLength(uuid);

        /// <summary>Sampling rate stored by the clip.</summary>
        public float sampleRate => ManagedRuntimeContext.AnimationClipGetSampleRate(uuid);

        /// <summary>Whether the clip stores deltas from a reference pose.</summary>
        public bool isAdditive => ManagedRuntimeContext.AnimationClipGetIsAdditive(uuid);
    }
}
