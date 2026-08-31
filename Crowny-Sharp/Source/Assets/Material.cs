namespace Crowny
{
    /// <summary>Controls how a material contributes to the scene color and transparency passes.</summary>
    public enum AlphaMode
    {
        Opaque = 0,
        Mask = 1,
        Premultiplied = 2,
        Additive = 3,
        WeightedOIT = 4
    }

    /// <summary>Built-in starting points for Crowny's toon material controls.</summary>
    public enum ToonMaterialPreset
    {
        Classic = 0,
        Soft = 1,
        Hatched = 2
    }

    public class Material : Asset
    {
        /// <summary>Gets or sets the explicit alpha routing override. Null keeps shader-based inference.</summary>
        public AlphaMode? AlphaModeOverride
        {
            get => HasAlphaModeOverride ? (AlphaMode)ManagedRuntimeContext.MaterialGetAlphaMode(m_ManagedUuid) : (AlphaMode?)null;
            set
            {
                if (value.HasValue)
                    ManagedRuntimeContext.MaterialSetAlphaMode(m_ManagedUuid, (int)value.Value);
                else
                    ClearAlphaModeOverride();
            }
        }

        /// <summary>Returns true when this material overrides shader-based alpha routing.</summary>
        public bool HasAlphaModeOverride => ManagedRuntimeContext.MaterialHasAlphaModeOverride(m_ManagedUuid);

        /// <summary>Restores shader-based alpha routing.</summary>
        public void ClearAlphaModeOverride() => ManagedRuntimeContext.MaterialClearAlphaModeOverride(m_ManagedUuid);

        /// <summary>
        /// Applies a toon preset while retaining the material's assigned textures.
        /// Returns false when the material does not use a compatible toon shader.
        /// </summary>
        public bool ApplyToonPreset(ToonMaterialPreset preset) =>
            ManagedRuntimeContext.MaterialApplyToonPreset(m_ManagedUuid, (int)preset);

        public void SetFloat(string name, float value) => ManagedRuntimeContext.MaterialSetFloat(m_ManagedUuid, name, value);
        public void SetVector2(string name, Vector2 value) => ManagedRuntimeContext.MaterialSetVector2(m_ManagedUuid, name, value);
        public void SetInt(string name, int value) => ManagedRuntimeContext.MaterialSetInt(m_ManagedUuid, name, value);
        public void SetColor(string name, Color value) => ManagedRuntimeContext.MaterialSetColor(m_ManagedUuid, name, value);
        public void SetVector3(string name, Vector3 value) => ManagedRuntimeContext.MaterialSetVector3(m_ManagedUuid, name, value);
        public void SetMatrix(string name, Matrix4 value) => ManagedRuntimeContext.MaterialSetMatrix(m_ManagedUuid, name, value);
        public void SetTexture(string name, Texture value) =>
            ManagedRuntimeContext.MaterialSetTexture(m_ManagedUuid, name, value != null ? value.uuid : UUID.Empty);
    }
}
