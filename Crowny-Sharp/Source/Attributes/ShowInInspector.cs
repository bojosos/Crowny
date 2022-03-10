using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to force show a field/property in the inspector. These attributes are not serialized and will be reset to their default values when the assembly is loaded.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Class | AttributeTargets.Struct)]
    public class ShowInInspector : Attribute
    {

    }
}