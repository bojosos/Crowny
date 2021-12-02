using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class Camera : Component
    {
        public Camera camera
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }
    }
}
