using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to signify that a field/property should not be null.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class NotNull : Attribute
    {

    }
}