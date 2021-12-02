using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
	public class Entity : ScriptObject
	{
		/// <summary>
		/// The name of the game object.
		/// </summary>
		/// <value>The game object name.</value>
		public string Name
		{
			get { return Internal_GetName(m_InternalPtr); }
			set { Internal_SetName(m_InternalPtr, value); }
		}

		/// <summary>
		/// The parent of the game object.
		/// </summary>
		/// <value>The parent game object.</value>
		public Entity parent
		{
			get { return Internal_GetParent(m_InternalPtr); }
			//set { Internal_SetParent(m_InternalPtr, value); }
		}

		/// <summary>
		/// The transform component of the game object.
		/// </summary>
		/// <value>Transform component.</value>
		public Transform transform
		{
			get { return Internal_GetTransform(m_InternalPtr); }
		}

		/// <summary>
        /// Searches for a game object by its name.
        /// </summary>
        /// <param name="name">The name of the game object.</param>
        /// <returns>The object if found, nullptr otherwise.</returns>
        public static Entity FindByName(string name)
        {
           return Internal_FindByName(name);
        }

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern string Internal_GetName(IntPtr internalPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetName(IntPtr internalPtr, string value);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetParent(IntPtr internalPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Transform Internal_GetTransform(IntPtr internalPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity Internal_FindByName(string name);
	}

}
