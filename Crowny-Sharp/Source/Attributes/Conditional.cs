using System;

namespace Crowny
{
    /// <summary>Shared settings for inspector conditions.</summary>
    public abstract class ConditionalAttribute : Attribute
    {
        protected ConditionalAttribute(string condition)
        {
            if (string.IsNullOrEmpty(condition))
                throw new ArgumentException("A condition member name is required.", "condition");
            Condition = condition;
        }

        protected ConditionalAttribute(string condition, object optionalValue) : this(condition)
        {
            Value = optionalValue;
            HasValue = true;
        }

        /// <summary>Gets the field, property, or parameterless method that supplies the condition.</summary>
        public string Condition { get; private set; }

        /// <summary>Gets the value that the condition member must equal.</summary>
        public object Value { get; private set; }

        /// <summary>Gets whether the condition compares against an explicit value.</summary>
        public bool HasValue { get; private set; }

        internal bool AnimateVisibility { get; set; }
    }

    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = true)]
    public sealed class ShowIf : ConditionalAttribute
    {
        public ShowIf(string condition, bool animate = true) : base(condition) { Animate = animate; }
        public ShowIf(string condition, object optionalValue, bool animate = true) : base(condition, optionalValue) { Animate = animate; }

        /// <summary>Gets or sets whether the editor animates visibility changes.</summary>
        public bool Animate { get { return AnimateVisibility; } set { AnimateVisibility = value; } }
    }

    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = true)]
    public sealed class HideIf : ConditionalAttribute
    {
        public HideIf(string condition, bool animate = true) : base(condition) { Animate = animate; }
        public HideIf(string condition, object optionalValue, bool animate = true) : base(condition, optionalValue) { Animate = animate; }

        /// <summary>Gets or sets whether the editor animates visibility changes.</summary>
        public bool Animate { get { return AnimateVisibility; } set { AnimateVisibility = value; } }
    }

    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = true)]
    public sealed class EnableIf : ConditionalAttribute
    {
        public EnableIf(string condition) : base(condition) { }
        public EnableIf(string condition, object optionalValue) : base(condition, optionalValue) { }
    }

    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = true)]
    public sealed class DisableIf : ConditionalAttribute
    {
        public DisableIf(string condition) : base(condition) { }
        public DisableIf(string condition, object optionalValue) : base(condition, optionalValue) { }
    }
}
