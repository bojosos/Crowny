using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class GameObject : ScriptObject
    {

        /// <summary>
        /// Searches for a game object by its name.
        /// </summary>
        /// <param name="name">The name of the game object.</param>
        /// <returns>The object if found, nullptr otherwise.</returns>
        public static GameObject Find(string name)
        {
           return Internal_Find(name);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject Internal_Find(string name);
      }
}
