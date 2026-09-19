using System;

namespace Crowny
{
    public class SpriteRendererComponent : Component
    {
        /// <summary>Optional atlas used to resolve Sprite. An absent atlas entry suppresses drawing.</summary>
        public SpriteAtlas Atlas
        {
            get => ManagedRuntimeContext.CreateAsset<SpriteAtlas>(ManagedRuntimeContext.SpriteRendererGetAtlas(EntityId));
            set => ManagedRuntimeContext.SpriteRendererSetAtlas(EntityId, value != null ? value.uuid : UUID.Empty);
        }
        /// <summary>Authored sprite. Null uses the legacy Texture and component geometry.</summary>
        public Sprite Sprite
        {
            get => ManagedRuntimeContext.CreateAsset<Sprite>(ManagedRuntimeContext.SpriteRendererGetSprite(EntityId));
            set => ManagedRuntimeContext.SpriteRendererSetSprite(EntityId, value?.uuid ?? UUID.Empty);
        }

        /// <summary>Use the sprite asset's pixel scale and dimensions instead of Size. Defaults to true.</summary>
        public bool UseSpriteSize
        {
            get => ManagedRuntimeContext.SpriteRendererGetUseSpriteSize(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetUseSpriteSize(EntityId, value);
        }

        /// <summary>Use the sprite asset's pivot instead of Pivot. Defaults to true.</summary>
        public bool UseSpritePivot
        {
            get => ManagedRuntimeContext.SpriteRendererGetUseSpritePivot(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetUseSpritePivot(EntityId, value);
        }

        /// <summary>Legacy full-texture image, used when Sprite is null.</summary>
        public Texture Texture
        {
            get { return ManagedRuntimeContext.CreateAsset<Texture>(ManagedRuntimeContext.SpriteRendererGetTexture(EntityId)); }
            set { ManagedRuntimeContext.SpriteRendererSetTexture(EntityId, value?.uuid ?? UUID.Empty); }
        }

        public Color Color
        {
            get { return ManagedRuntimeContext.SpriteRendererGetColor(EntityId); }
            set { ManagedRuntimeContext.SpriteRendererSetColor(EntityId, value); }
        }

        /// <summary>Sprite dimensions in local units, before the entity transform. Nonpositive dimensions suppress drawing.</summary>
        public Vector2 Size
        {
            get => ManagedRuntimeContext.SpriteRendererGetSize(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetSize(EntityId, value);
        }

        /// <summary>Normalized pivot. (0.5, 0.5) centers the image; (0, 0) anchors its lower-left corner.</summary>
        public Vector2 Pivot
        {
            get => ManagedRuntimeContext.SpriteRendererGetPivot(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetPivot(EntityId, value);
        }

        /// <summary>Normalized texture endpoints (u0, v0, u1, v1), each within [0, 1]. Empty or reversed regions suppress drawing.</summary>
        public Vector4 UvRect
        {
            get => ManagedRuntimeContext.SpriteRendererGetUvRect(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetUvRect(EntityId, value);
        }

        /// <summary>Mirrors the image horizontally around its pivot without changing the entity transform or colliders.</summary>
        public bool FlipX
        {
            get => ManagedRuntimeContext.SpriteRendererGetFlipX(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetFlipX(EntityId, value);
        }

        /// <summary>Mirrors the image vertically around its pivot without changing the entity transform or colliders.</summary>
        public bool FlipY
        {
            get => ManagedRuntimeContext.SpriteRendererGetFlipY(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetFlipY(EntityId, value);
        }

        /// <summary>Whether this sprite participates in rendering and picking. Other components remain active.</summary>
        public bool Visible
        {
            get => ManagedRuntimeContext.SpriteRendererGetVisible(EntityId);
            set => ManagedRuntimeContext.SpriteRendererSetVisible(EntityId, value);
        }

        /// <summary>Primary stable ordering key shared by sprites and text.</summary>
        public int SortingLayer
        {
            get { return ManagedRuntimeContext.SpriteRendererGetSortingLayer(EntityId); }
            set { ManagedRuntimeContext.SpriteRendererSetSortingLayer(EntityId, value); }
        }

        /// <summary>Ordering key within SortingLayer. Lower values render first.</summary>
        public int OrderInLayer
        {
            get { return ManagedRuntimeContext.SpriteRendererGetOrderInLayer(EntityId); }
            set { ManagedRuntimeContext.SpriteRendererSetOrderInLayer(EntityId, value); }
        }

        [Obsolete("Use Texture instead.")]
        public Texture texture { get { return Texture; } set { Texture = value; } }
        [Obsolete("Use Color instead.")]
        public Color color { get { return Color; } set { Color = value; } }
        [Obsolete("Use SortingLayer instead.")]
        public int sortingLayer { get { return SortingLayer; } set { SortingLayer = value; } }
        [Obsolete("Use OrderInLayer instead.")]
        public int orderInLayer { get { return OrderInLayer; } set { OrderInLayer = value; } }

    }
}
