using System;
using System.Runtime.CompilerServices;
using Crowny;

namespace Sandbox
{
    /// <summary>Exercises native callback exception reporting in the Mono integration tests.</summary>
    public class MonoExceptionProbe : EntityBehaviour
    {
        [MethodImpl(MethodImplOptions.NoInlining)]
        private void Update()
        {
            throw new InvalidOperationException("Mono callback diagnostic probe.");
        }
    }
}
