using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public struct LayerMask
    {
        
        public static implicit operator LayerMask(int val)
        {
            LayerMask mask = new LayerMask();
            mask.value = val;
            return mask;
        }
        
        public int value { get; set; }

        public static string LayerToName(int layer) => Internal_LayerToName(layer);

        public static int NameToLayer(string name) => Internal_NameToLayer(name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_LayerToName(int layer);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_NameToLayer(string name);
    }

}