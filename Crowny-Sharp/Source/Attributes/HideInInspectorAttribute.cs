using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to hide a field/property in the inspector
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class HideInInspector : Attribute
    {

    }
}