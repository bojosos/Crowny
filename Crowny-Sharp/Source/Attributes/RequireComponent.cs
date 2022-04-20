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
        // Note: This is a list since normal arrays aren't really supported yet.
        private List<Type> components;
        #pragma warning restore 0414

        public RequireComponent(params Type[] components)
        {
            foreach (var type in components)
            {
                if (type.BaseType != null && type.BaseType != typeof(Component))
                    throw new Exception("RequireComponent can only be used with builtin Components.");

            }
            this.components = new List<Type>(components);
        }
    }
}