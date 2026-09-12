using Crowny;

namespace Sandbox
{
    /// <summary>
    /// Records the order of lifecycle callbacks for the native lifecycle tests. Each entry carries a process-wide
    /// sequence number so ordering across several probe instances can be verified from the captured state.
    /// OnDestroy cannot be observed through the destroyed instance, so it appends the final log to the name of the
    /// entity called "LifecycleProbeSink" when one exists in the active scene.
    /// </summary>
    public class LifecycleProbe : EntityBehaviour
    {
        private static int s_Sequence;
        private static Entity s_Sink;

        public string log = "";

        void Awake()
        {
            if (s_Sink == null)
                s_Sink = Entity.FindByName("LifecycleProbeSink");
            Append("Awake");
        }

        void Start() { Append("Start"); }

        void Update() { Append("Update"); }

        void LateUpdate() { Append("LateUpdate"); }

        void FixedUpdate() { Append("FixedUpdate"); }

        void OnDestroy()
        {
            Append("OnDestroy");
            // FindByName is an exact match, and the first OnDestroy already
            // appends to the sink's name, so the sink is resolved once in Awake.
            if (s_Sink != null)
                s_Sink.name = s_Sink.name + "|" + log;
        }

        private void Append(string callback)
        {
            string entry = callback + "@" + (++s_Sequence);
            log = log.Length == 0 ? entry : log + "," + entry;
        }
    }
}
