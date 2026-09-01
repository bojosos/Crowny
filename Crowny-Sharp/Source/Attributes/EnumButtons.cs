using System;

namespace Crowny
{
    /// <summary>
    /// Displays an enum as a group of buttons. Enums marked with <see cref="FlagsAttribute"/>
    /// allow multiple buttons to be selected.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = false)]
    public sealed class EnumButtons : Attribute
    {
        public EnumButtons(bool includeObsolete = false)
        {
            this.includeObsolete = includeObsolete;
        }

        /// <summary>
        /// Controls whether obsolete enum values remain visible. Values marked as obsolete errors are always hidden.
        /// </summary>
        public bool includeObsolete;

        /// <summary>
        /// Pascal-case alias for <see cref="includeObsolete"/>.
        /// </summary>
        public bool IncludeObsolete
        {
            get { return includeObsolete; }
            set { includeObsolete = value; }
        }
    }
}
