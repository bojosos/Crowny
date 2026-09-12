namespace Crowny
{
    /// <summary>Projection volume used by a decal.</summary>
    public enum DecalProjection { Box, Cylinder }
    /// <summary>Receiver filtering for a decal.</summary>
    public enum DecalTargetMode { Layers, Entity, Subtree }

    /// <summary>Projects a material onto matching surfaces within a box or tapered cylindrical shell.</summary>
    public class DecalComponent : Component
    {
        /// <summary>The reusable decal material. Instance controls do not modify this asset.</summary>
        public Material Material
        {
            get => ManagedRuntimeContext.CreateAsset<Material>(ManagedRuntimeContext.DecalGetReference(EntityId, 0));
            set => ManagedRuntimeContext.DecalSetReference(EntityId, 0, value != null ? value.uuid : UUID.Empty);
        }
        /// <summary>The target entity UUID, used by Entity and Subtree targeting.</summary>
        public UUID Target
        {
            get => ManagedRuntimeContext.DecalGetReference(EntityId, 1);
            set => ManagedRuntimeContext.DecalSetReference(EntityId, 1, value);
        }
        /// <summary>The projection shape.</summary>
        public DecalProjection Projection { get => (DecalProjection)GetInt(0); set => SetInt(0, (int)value); }
        /// <summary>How receiver layers and the target entity filter surfaces.</summary>
        public DecalTargetMode TargetMode { get => (DecalTargetMode)GetInt(1); set => SetInt(1, (int)value); }
        /// <summary>Higher sort orders are applied later.</summary>
        public int SortOrder { get => GetInt(2); set => SetInt(2, value); }
        /// <summary>Independent decal receiver layer mask.</summary>
        public uint ReceiverLayers { get => unchecked((uint)GetInt(3)); set => SetInt(3, unchecked((int)value)); }
        /// <summary>Whether this decal is rendered.</summary>
        public bool Enabled { get => GetInt(4) != 0; set => SetInt(4, value ? 1 : 0); }
        /// <summary>Preserve UV density when resizing the volume in the editor.</summary>
        public bool PreserveTexelDensity { get => GetInt(5) != 0; set => SetInt(5, value ? 1 : 0); }
        /// <summary>Destroy the owning entity after lifetime and fade-out expire.</summary>
        public bool DestroyOwnerOnExpiry { get => GetInt(6) != 0; set => SetInt(6, value ? 1 : 0); }
        /// <summary>Instance tint and coverage multiplier.</summary>
        public Color Tint { get => GetVector(0); set => SetVector(0, value); }
        /// <summary>Local projection center.</summary>
        public Vector3 Offset { get { Color v = GetVector(1); return new Vector3(v.r, v.g, v.b); } set => SetVector(1, new Color(value.x, value.y, value.z, 0)); }
        /// <summary>Local box width, height, and depth.</summary>
        public Vector3 Size { get { Color v = GetVector(2); return new Vector3(v.r, v.g, v.b); } set => SetVector(2, new Color(value.x, value.y, value.z, 0)); }
        /// <summary>UV scale. Negative values flip the artwork.</summary>
        public Vector2 UVScale { get { Color v = GetVector(3); return new Vector2(v.r, v.g); } set { Color v = GetVector(3); v.r = value.x; v.g = value.y; SetVector(3, v); } }
        /// <summary>UV translation.</summary>
        public Vector2 UVOffset { get { Color v = GetVector(3); return new Vector2(v.b, v.a); } set { Color v = GetVector(3); v.b = value.x; v.a = value.y; SetVector(3, v); } }
        /// <summary>BottomRadius in local units.</summary>
        public float BottomRadius { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 0); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 0, value); }
        /// <summary>TopRadius in local units.</summary>
        public float TopRadius { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 1); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 1, value); }
        /// <summary>Height in local units.</summary>
        public float Height { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 2); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 2, value); }
        /// <summary>ShellThickness in local units.</summary>
        public float ShellThickness { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 3); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 3, value); }
        /// <summary>Arc in degrees.</summary>
        public float Arc { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 4); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 4, value); }
        /// <summary>SeamRotation in degrees.</summary>
        public float SeamRotation { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 5); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 5, value); }
        /// <summary>Opacity for this decal instance.</summary>
        public float Opacity { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 6); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 6, value); }
        /// <summary>UVRotation in degrees.</summary>
        public float UVRotation { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 7); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 7, value); }
        /// <summary>EdgeFeather in local units.</summary>
        public float EdgeFeather { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 8); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 8, value); }
        /// <summary>DepthFeather in local units.</summary>
        public float DepthFeather { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 9); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 9, value); }
        /// <summary>AngleFadeStart in degrees.</summary>
        public float AngleFadeStart { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 10); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 10, value); }
        /// <summary>AngleFadeEnd in degrees.</summary>
        public float AngleFadeEnd { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 11); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 11, value); }
        /// <summary>DistanceFadeStart for this decal instance.</summary>
        public float DistanceFadeStart { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 12); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 12, value); }
        /// <summary>DistanceFadeEnd for this decal instance.</summary>
        public float DistanceFadeEnd { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 13); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 13, value); }
        /// <summary>FadeIn in simulation seconds. Zero lifetime is persistent.</summary>
        public float FadeIn { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 14); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 14, value); }
        /// <summary>Lifetime in simulation seconds. Zero lifetime is persistent.</summary>
        public float Lifetime { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 15); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 15, value); }
        /// <summary>FadeOut in simulation seconds. Zero lifetime is persistent.</summary>
        public float FadeOut { get => ManagedRuntimeContext.DecalGetFloat(EntityId, 16); set => ManagedRuntimeContext.DecalSetFloat(EntityId, 16, value); }
        /// <summary>Enable the decal and restart its fade and lifetime clock.</summary>
        public void RestartLifetime() => ManagedRuntimeContext.DecalLifetime(EntityId, true);
        /// <summary>Freeze the lifetime clock at its current age.</summary>
        public void StopLifetime() => ManagedRuntimeContext.DecalLifetime(EntityId, false);
        private int GetInt(uint field) => ManagedRuntimeContext.DecalGetInt(EntityId, field);
        private void SetInt(uint field, int value) => ManagedRuntimeContext.DecalSetInt(EntityId, field, value);
        private Color GetVector(uint field) => ManagedRuntimeContext.DecalGetVector(EntityId, field);
        private void SetVector(uint field, Color value) => ManagedRuntimeContext.DecalSetVector(EntityId, field, value);
    }
}

