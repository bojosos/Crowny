using System;
using Crowny;

namespace Sandbox
{
    public abstract class ScriptLookupTarget : EntityBehaviour
    {
        public int marker;
    }

    public sealed class ScriptLookupFirst : ScriptLookupTarget { }

    public sealed class ScriptLookupSecond : ScriptLookupTarget { }

    public sealed class ScriptLookupFailing : ScriptLookupTarget
    {
        public ScriptLookupFailing() { throw new InvalidOperationException("ScriptLookupFailing construction failed."); }
    }

    // Native integration tests attach several instances and dispatch Start again after removals and reloads.
    public sealed class ScriptLookupProbe : EntityBehaviour
    {
        public int firstMarker = -1;
        public int secondMarker = -1;
        public int baseMarker = -1;
        public bool hasFailing;

        private void Start()
        {
            ScriptLookupFirst first = entity.GetComponent<ScriptLookupFirst>();
            ScriptLookupSecond second = entity.GetComponent<ScriptLookupSecond>();
            ScriptLookupTarget target = entity.GetComponent<ScriptLookupTarget>();
            firstMarker = first == null ? -1 : first.marker;
            secondMarker = second == null ? -1 : second.marker;
            baseMarker = target == null ? -1 : target.marker;
            hasFailing = entity.GetComponent<ScriptLookupFailing>() != null;
        }
    }
}
