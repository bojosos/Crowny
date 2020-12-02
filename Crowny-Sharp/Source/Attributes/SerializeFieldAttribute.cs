using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to make a field/property as serializable
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class SerializeField : Attribute
    {

    }
}