using System;

namespace Crowny
{
    /// <summary>
    /// Label lets you specify the inspector label for a field.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = false)]
    public class Label : Attribute
    {
        public Label(string label)
        {
            this.label = label;
        }

        /// <summary>
        /// The text shown by the inspector. This does not change the serialized member name.
        /// </summary>
        public string label { get; private set; }
    }
}
