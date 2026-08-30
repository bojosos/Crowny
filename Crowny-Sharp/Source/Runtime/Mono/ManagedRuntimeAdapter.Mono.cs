#if CROWNY_MONO
using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    internal static partial class ManagedRuntimeAdapter
    {
        private static Component ResolveScriptComponentImplementation(UUID entity, Type type)
        {
            return Internal_ResolveScriptComponent(ref entity, type);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component Internal_ResolveScriptComponent(ref UUID entity, Type type);
    }
}
#endif
