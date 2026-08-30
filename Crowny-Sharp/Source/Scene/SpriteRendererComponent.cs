using System;

namespace Crowny
{
    public class SpriteRendererComponent : Component
    {
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
