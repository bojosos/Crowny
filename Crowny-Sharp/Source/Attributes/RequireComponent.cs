using System;
using System.Collections.Generic;

namespace Crowny
{
    /// <summary>
    /// RequireComponent lets you automatically add components.
    /// </summary>
    [AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
    public class RequireComponent : Attribute
    {
        #pragma warning disable 0414
        
        private List<Type> components;
        #pragma warning restore 0414

        public RequireComponent(params Type[] components)
        {
            this.components = new List<Type>(components);
            List<int> toRemove = new List<int>();
            for (int i = 0; i < this.components.Count; i++)
            {
                if (!this.components[i].IsSubclassOf(typeof(Component)))
                {
                    Debug.LogWarning("RequireComponent: " + this.components[i].Name + " is not a Component.");
                    toRemove.Add(i);
                }
            }
            for (int i = toRemove.Count - 1; i >= 0; i--) // go from back to front to avoid messing up the indices
                this.components.RemoveAt(i);
        }
    }
}