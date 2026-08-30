using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>Owns the synchronous pinning rules used by generated bulk-data bindings.</summary>
    internal static class ManagedArrayInterop
    {
        internal static T[] Read<T>(uint expectedCount, Func<IntPtr, uint, uint> copy) where T : struct
        {
            T[] values = new T[checked((int)expectedCount)];
            uint actualCount = WithPinned(values, copy);
            while (actualCount > (uint)values.Length)
            {
                values = new T[checked((int)actualCount)];
                actualCount = WithPinned(values, copy);
            }
            if (actualCount < (uint)values.Length)
                Array.Resize(ref values, checked((int)actualCount));
            return values;
        }

        internal static T[] Query<T>(uint initialCapacity, Func<IntPtr, uint, uint> query) where T : struct
        {
            return Read<T>(initialCapacity, query);
        }

        internal static int WriteNonAlloc<T>(T[] values, Func<IntPtr, uint, uint> write) where T : struct
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if (values.Length == 0)
                return 0;
            uint available = WithPinned(values, write);
            return checked((int)Math.Min(available, (uint)values.Length));
        }

        internal static TResult WithPinned<T, TResult>(T[] values, Func<IntPtr, uint, TResult> operation) where T : struct
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if (values.Length == 0)
                return operation(IntPtr.Zero, 0);
            GCHandle handle = GCHandle.Alloc(values, GCHandleType.Pinned);
            try
            {
                return operation(handle.AddrOfPinnedObject(), checked((uint)values.Length));
            }
            finally
            {
                handle.Free();
            }
        }

        internal static void WithPinned<T>(T[] values, Action<IntPtr, uint> operation) where T : struct
        {
            if (values == null)
                throw new ArgumentNullException("values");
            if (values.Length == 0)
            {
                operation(IntPtr.Zero, 0);
                return;
            }
            GCHandle handle = GCHandle.Alloc(values, GCHandleType.Pinned);
            try
            {
                operation(handle.AddrOfPinnedObject(), checked((uint)values.Length));
            }
            finally
            {
                handle.Free();
            }
        }
    }
}
