using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{

    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit2D
    {
        public Vector2 Point;
        public Vector2 Normal;
        public float Fraction;
        public uint EntityId;
    }

    public static class Physics2D
    {
        public static RaycastHit2D[] Raycast(Vector2 origin, Vector2 direction, float distance = float.MaxValue, uint layerMask = 0xFFFFFFFF)
        {
            Internal_Raycast(ref origin, ref direction, distance, layerMask, out RaycastHit2D[] results);
            return results ?? new RaycastHit2D[0];
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Raycast(ref Vector2 origin, ref Vector2 direction, float distance, uint layerMask, out RaycastHit2D[] results);
    }

}
