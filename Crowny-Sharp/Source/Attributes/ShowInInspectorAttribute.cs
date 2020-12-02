using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to show a field/property in the inspector
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class ShowInInspector : Attribute
    {

    }
}