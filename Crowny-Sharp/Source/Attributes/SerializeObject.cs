using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to mark an object as serializable.
    /// </summary>
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
    public sealed class SerializeObject : Attribute
    {

    }
}