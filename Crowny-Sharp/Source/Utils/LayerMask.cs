using System;

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

        public static string LayerToName(int layer)
        {
            string name = ManagedRuntimeContext.LayerMaskGetName(layer);
            return name.Length == 0 ? null : name;
        }

        public static int NameToLayer(string name) => ManagedRuntimeContext.LayerMaskGetLayer(name);
    }

}
