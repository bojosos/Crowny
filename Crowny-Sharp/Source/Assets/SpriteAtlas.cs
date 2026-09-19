namespace Crowny
{
    /// <summary>Packed sprite pages addressed by the original Sprite asset identities.</summary>
    public class SpriteAtlas : Asset
    {
        /// <summary>Number of packed texture pages.</summary>
        public uint PageCount => ManagedRuntimeContext.SpriteAtlasGetPageCount(uuid);
        /// <summary>Number of sprite entries.</summary>
        public uint EntryCount => ManagedRuntimeContext.SpriteAtlasGetEntryCount(uuid);
    }
}
