using System;
using System.Collections.Generic;

namespace Crowny
{
    /// <summary>
    /// Label lets you specify the inspector label for a field.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class Label : Attribute
    {
        
#pragma warning disable 0414
        private string label;
#pragma warning restore 0414

        public Label(string label)
        {
            this.label = label;
        }
    }
}