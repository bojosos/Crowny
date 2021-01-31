using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class CameraComponent : Component
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
