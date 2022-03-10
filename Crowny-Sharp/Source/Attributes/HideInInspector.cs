using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to force hide a field/property in the inspecto.r
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Class | AttributeTargets.Struct)]
    public class HideInInspector : Attribute
    {

    }
}