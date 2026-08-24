using System;

namespace Crowny
{
    /// <summary>
    /// Preserves a serialized value when a field or property is renamed.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class FormerlySerializedAs : Attribute
    {
#pragma warning disable 0414
        private readonly string oldName;
#pragma warning restore 0414

        public FormerlySerializedAs(string oldName)
        {
            this.oldName = oldName;
        }
    }
}
