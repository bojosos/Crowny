using System;

namespace Crowny
{
    /// <summary>Invokes a method after the inspector commits a changed field or property.</summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = true)]
    public sealed class OnValueChanged : Attribute
    {
        public OnValueChanged(string action, bool includeChildren = false)
        {
            if (string.IsNullOrEmpty(action))
                throw new ArgumentException("An action method name is required.", "action");
            Action = action;
            IncludeChildren = includeChildren;
        }

        /// <summary>Gets the parameterless method, or single-value method, to invoke.</summary>
        public string Action { get; private set; }

        /// <summary>Gets whether edits to collection elements or object children invoke the action.</summary>
        public bool IncludeChildren { get; private set; }

        /// <summary>Gets or sets whether the action runs when the inspector first creates the field UI.</summary>
        public bool InvokeOnInitialize { get; set; }

        /// <summary>Gets or sets whether the action may run after undo and redo operations.</summary>
        public bool InvokeOnUndoRedo { get; set; } = true;
    }
}
