using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    
    [StructLayout(LayoutKind.Sequential)]
    public struct Collision2D
    {
        public Entity[] colliders;
        public Vector2[] points;
    }

    public class Collider2D : Component
    { 
        public Vector2 offset { get; set; }
        // public Bounds bounds { get; }
    }
}
