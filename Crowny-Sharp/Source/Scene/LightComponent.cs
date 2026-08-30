using System;

namespace Crowny
{
    public enum LightType
    {
        Directional,
        Point,
        Spot
    }

    public enum LightShadowMode
    {
        Disabled,
        Hard,
        Soft
    }

    /// <summary>A physically based directional, point, or spot light.</summary>
    public class LightComponent : Component
    {
        public LightType Type
        {
            get => (LightType)ManagedRuntimeContext.LightGetType(EntityId);
            set => ManagedRuntimeContext.LightSetType(EntityId, (int)value);
        }

        public Color Color
        {
            get => ManagedRuntimeContext.LightGetColor(EntityId);
            set => ManagedRuntimeContext.LightSetColor(EntityId, value);
        }

        /// <summary>Lux for directional lights; lumens for point and spot lights.</summary>
        public float Intensity
        {
            get => ManagedRuntimeContext.LightGetIntensity(EntityId);
            set => ManagedRuntimeContext.LightSetIntensity(EntityId, value);
        }

        public float Range
        {
            get => ManagedRuntimeContext.LightGetRange(EntityId);
            set => ManagedRuntimeContext.LightSetRange(EntityId, value);
        }

        /// <summary>Inner spot cone angle in degrees.</summary>
        public float SpotInnerAngle
        {
            get => ManagedRuntimeContext.LightGetSpotInnerAngle(EntityId);
            set => ManagedRuntimeContext.LightSetSpotInnerAngle(EntityId, value);
        }

        /// <summary>Outer spot cone angle in degrees.</summary>
        public float SpotOuterAngle
        {
            get => ManagedRuntimeContext.LightGetSpotOuterAngle(EntityId);
            set => ManagedRuntimeContext.LightSetSpotOuterAngle(EntityId, value);
        }

        public float SourceRadius
        {
            get => ManagedRuntimeContext.LightGetSourceRadius(EntityId);
            set => ManagedRuntimeContext.LightSetSourceRadius(EntityId, value);
        }

        public bool UseColorTemperature
        {
            get => ManagedRuntimeContext.LightGetUseColorTemperature(EntityId);
            set => ManagedRuntimeContext.LightSetUseColorTemperature(EntityId, value);
        }

        /// <summary>Black-body color temperature in kelvin, clamped to 1000-40000 K.</summary>
        public float Temperature
        {
            get => ManagedRuntimeContext.LightGetTemperature(EntityId);
            set => ManagedRuntimeContext.LightSetTemperature(EntityId, value);
        }

        public uint VisibilityLayers
        {
            get => ManagedRuntimeContext.LightGetVisibilityLayers(EntityId);
            set => ManagedRuntimeContext.LightSetVisibilityLayers(EntityId, value);
        }

        public bool Enabled
        {
            get => ManagedRuntimeContext.LightGetEnabled(EntityId);
            set => ManagedRuntimeContext.LightSetEnabled(EntityId, value);
        }

        public bool AffectDiffuse
        {
            get => ManagedRuntimeContext.LightGetAffectDiffuse(EntityId);
            set => ManagedRuntimeContext.LightSetAffectDiffuse(EntityId, value);
        }

        public bool AffectSpecular
        {
            get => ManagedRuntimeContext.LightGetAffectSpecular(EntityId);
            set => ManagedRuntimeContext.LightSetAffectSpecular(EntityId, value);
        }

        public bool Volumetric
        {
            get => ManagedRuntimeContext.LightGetVolumetric(EntityId);
            set => ManagedRuntimeContext.LightSetVolumetric(EntityId, value);
        }

        public LightShadowMode Shadows
        {
            get => (LightShadowMode)ManagedRuntimeContext.LightGetShadows(EntityId);
            set => ManagedRuntimeContext.LightSetShadows(EntityId, (int)value);
        }

        public float ShadowBias
        {
            get => ManagedRuntimeContext.LightGetShadowBias(EntityId);
            set => ManagedRuntimeContext.LightSetShadowBias(EntityId, value);
        }

        public float ShadowNormalBias
        {
            get => ManagedRuntimeContext.LightGetShadowNormalBias(EntityId);
            set => ManagedRuntimeContext.LightSetShadowNormalBias(EntityId, value);
        }

        public float ShadowNearPlane
        {
            get => ManagedRuntimeContext.LightGetShadowNearPlane(EntityId);
            set => ManagedRuntimeContext.LightSetShadowNearPlane(EntityId, value);
        }

        public float ShadowImportance
        {
            get => ManagedRuntimeContext.LightGetShadowImportance(EntityId);
            set => ManagedRuntimeContext.LightSetShadowImportance(EntityId, value);
        }

        public uint ShadowResolution
        {
            get => ManagedRuntimeContext.LightGetShadowResolution(EntityId);
            set => ManagedRuntimeContext.LightSetShadowResolution(EntityId, value);
        }

        public bool CacheStaticShadowCasters
        {
            get => ManagedRuntimeContext.LightGetCacheStaticShadowCasters(EntityId);
            set => ManagedRuntimeContext.LightSetCacheStaticShadowCasters(EntityId, value);
        }

        [Obsolete("Use Type instead.")] public LightType type { get => Type; set => Type = value; }
        [Obsolete("Use Color instead.")] public Color color { get => Color; set => Color = value; }
        [Obsolete("Use Intensity instead.")] public float intensity { get => Intensity; set => Intensity = value; }
        [Obsolete("Use Range instead.")] public float range { get => Range; set => Range = value; }
        [Obsolete("Use SpotInnerAngle instead.")] public float spotInnerAngle { get => SpotInnerAngle; set => SpotInnerAngle = value; }
        [Obsolete("Use SpotOuterAngle instead.")] public float spotOuterAngle { get => SpotOuterAngle; set => SpotOuterAngle = value; }
        [Obsolete("Use Enabled instead.")] public bool enabled { get => Enabled; set => Enabled = value; }
        [Obsolete("Use Shadows instead.")] public LightShadowMode shadows { get => Shadows; set => Shadows = value; }

    }
}
