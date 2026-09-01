using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>Test-only probes for comparing the Mono internal-call and host-table dispatch mechanisms.</summary>
    internal static class ManagedBindingBenchmark
    {
        private static float sink;

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetValue();

        internal static long MeasureDirectInternalCall(int iterations)
        {
            float value = 0.0f;
            long start = Stopwatch.GetTimestamp();
            for (int index = 0; index < iterations; index++)
                value += Internal_GetValue();
            long elapsed = Stopwatch.GetTimestamp() - start;
            sink = value;
            return elapsed * 1_000_000_000L / Stopwatch.Frequency;
        }

        internal static long MeasureHostTableCall(int iterations)
        {
            float value = 0.0f;
            long start = Stopwatch.GetTimestamp();
            for (int index = 0; index < iterations; index++)
                value += ManagedRuntimeContext.TimeGetDeltaTime();
            long elapsed = Stopwatch.GetTimestamp() - start;
            sink = value;
            return elapsed * 1_000_000_000L / Stopwatch.Frequency;
        }
    }
}
