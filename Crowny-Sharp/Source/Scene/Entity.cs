using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    class Entity
    {

        public Entity()
        {
            Internal_Entity(this);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Entity(Entity e);
        
    }
}
