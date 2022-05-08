using System;
using System.Collections.Generic;

namespace Crowny
{
    /// <summary>
    /// Header lets you group fields in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class Header : Attribute
    {

#pragma warning disable 0414
        private string label;
        private bool collapsable;
#pragma warning restore 0414

        public Header(string label, bool collapsable = false)
        {
            this.label = label;
            this.collapsable = collapsable;
        }
    }
}