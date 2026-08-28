#if CROWNY_MONO
using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public partial class Entity
    {
        private UUID GetUuid()
        {
            Internal_GetUUID(m_InternalPtr, out UUID value);
            return value;
        }

        private Entity GetParent() => Internal_GetParent(m_InternalPtr);
        private void SetParent(Entity value) => Internal_SetParent(m_InternalPtr, value);
        private static Entity FindByNameBinding(string name) => Internal_FindByName(name);
        private T GetComponentBinding<T>() where T : Component => (T)Internal_GetComponent(m_InternalPtr, typeof(T));
        private bool HasComponentBinding<T>() where T : Component => Internal_HasComponent(m_InternalPtr, typeof(T));
        private T AddComponentBinding<T>() where T : Component => (T)Internal_AddComponent(m_InternalPtr, typeof(T));
        private void RemoveComponentBinding<T>() where T : Component => Internal_RemoveComponent(m_InternalPtr, typeof(T));

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Component Internal_GetComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_HasComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Component Internal_AddComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_RemoveComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetUUID(IntPtr parent, out UUID uuid);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_GetParent(IntPtr internalPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_SetParent(IntPtr internalPtr, Entity entity);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_FindByName(string name);
    }
}
#endif
