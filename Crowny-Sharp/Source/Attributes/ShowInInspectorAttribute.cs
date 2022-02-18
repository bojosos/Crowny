using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to force show a field/property in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Class | AttributeTargets.Struct)]
    public class ShowInInspector : Attribute
    {

    }
}