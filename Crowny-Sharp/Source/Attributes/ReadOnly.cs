using System;
using System.Collections.Generic;

namespace Crowny
{
    /// <summary>
    /// ReadOnly lets you mark a field as read only in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class ReadOnly : Attribute
    {

    }
}