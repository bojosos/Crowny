using System;

namespace Crowny
{
    /// <summary>
    /// Attribute used to prevent serialization of a field.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public class DontSerializeField : Attribute
    {

    }
}