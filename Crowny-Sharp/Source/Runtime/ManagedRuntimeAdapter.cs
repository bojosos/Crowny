using System;

namespace Crowny
{
    internal static partial class ManagedRuntimeAdapter
    {
        internal static Component ResolveScriptComponent(UUID entity, Type type)
        {
            return ResolveScriptComponentImplementation(entity, type);
        }
    }
}
