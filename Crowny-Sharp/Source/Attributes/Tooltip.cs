using System;

namespace Crowny
{
    /// <summary>
    /// Lets you show a tooltip for fields in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Enum, Inherited = true, AllowMultiple = false)]
    public class Tooltip : Attribute
    {
        public Tooltip(string tooltip)
        {
            this.tooltip = tooltip;
        }

        public string tooltip { get; private set; }
    }
}
