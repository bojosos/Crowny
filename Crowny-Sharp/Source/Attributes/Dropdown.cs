using System;

namespace Crowny
{
    /// <summary>
    /// Dropdown can be used to show a dropdown control in the inspector for booleans only for now.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class Dropdown : Attribute
    {

    }
}