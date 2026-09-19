namespace Crowny
{
    /// <summary>An authored texture region with a pivot and pixel scale. Edit its source asset in the editor.</summary>
    public class Sprite : Asset
    {
        /// <summary>The texture containing this sprite.</summary>
        public Texture Texture => ManagedRuntimeContext.CreateAsset<Texture>(ManagedRuntimeContext.SpriteGetTexture(uuid));
        /// <summary>Number of source pixels per world unit.</summary>
        public float PixelsPerUnit => ManagedRuntimeContext.SpriteGetPixelsPerUnit(uuid);
        /// <summary>Normalized pivot, measured from the lower-left corner.</summary>
        public Vector2 Pivot => ManagedRuntimeContext.SpriteGetPivot(uuid);
        /// <summary>Original pixel dimensions. A zero axis derives its dimension from the texture region.</summary>
        public Vector2 OriginalSize => ManagedRuntimeContext.SpriteGetOriginalSize(uuid);
        /// <summary>Resolved dimensions in world units, or zero while the texture is unavailable.</summary>
        public Vector2 Size => ManagedRuntimeContext.SpriteGetSize(uuid);
        /// <summary>Normalized texture endpoints (u0, v0, u1, v1).</summary>
        public Vector4 UvRect => ManagedRuntimeContext.SpriteGetUvRect(uuid);
        /// <summary>Authored left, bottom, right, and top borders in original pixels, reserved for nine-slice drawing.</summary>
        public Vector4 Borders => ManagedRuntimeContext.SpriteGetBorders(uuid);
    }
}
