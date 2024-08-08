using System;
using System.Collections.Generic;

namespace Crowny
{
    /// <summary>
    /// Lets you show a tooltip for fields in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Enum)]
    public class Tooltip : Attribute
    {
#pragma warning disable 0414
        private string tooltip;
#pragma warning restore 0414

        public Tooltip(string tooltip)
        {
            this.tooltip = tooltip;
        }
    }
}