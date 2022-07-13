using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to prevent serialization of a field.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class DontSerializeField : Attribute
    {

    }
}