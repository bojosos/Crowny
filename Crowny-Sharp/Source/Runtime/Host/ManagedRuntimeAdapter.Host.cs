#if !CROWNY_MONO
using System;

namespace Crowny
{
    internal static partial class ManagedRuntimeAdapter
    {
        private static Component ResolveScriptComponentImplementation(UUID entity, Type type)
        {
            return ManagedRuntimeContext.ResolveRegisteredScriptComponent(entity, type);
        }
    }
}
#endif
