namespace Crowny
{
    public class Texture : Asset
    {
        public uint width => ManagedRuntimeContext.TextureGetWidth(uuid);
        public uint height => ManagedRuntimeContext.TextureGetHeight(uuid);
    }
}
