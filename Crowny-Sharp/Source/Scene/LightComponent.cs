using System;
using System.Runtime.CompilerServices;

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
            get => Internal_GetType(m_InternalPtr);
            set => Internal_SetType(m_InternalPtr, value);
        }

        public Color Color
        {
            get { Internal_GetColor(m_InternalPtr, out Color value); return value; }
            set => Internal_SetColor(m_InternalPtr, ref value);
        }

        /// <summary>Lux for directional lights; lumens for point and spot lights.</summary>
        public float Intensity
        {
            get => Internal_GetIntensity(m_InternalPtr);
            set => Internal_SetIntensity(m_InternalPtr, value);
        }

        public float Range
        {
            get => Internal_GetRange(m_InternalPtr);
            set => Internal_SetRange(m_InternalPtr, value);
        }

        /// <summary>Inner spot cone angle in degrees.</summary>
        public float SpotInnerAngle
        {
            get => Internal_GetSpotInnerAngle(m_InternalPtr);
            set => Internal_SetSpotInnerAngle(m_InternalPtr, value);
        }

        /// <summary>Outer spot cone angle in degrees.</summary>
        public float SpotOuterAngle
        {
            get => Internal_GetSpotOuterAngle(m_InternalPtr);
            set => Internal_SetSpotOuterAngle(m_InternalPtr, value);
        }

        public float SourceRadius
        {
            get => Internal_GetSourceRadius(m_InternalPtr);
            set => Internal_SetSourceRadius(m_InternalPtr, value);
        }

        public bool UseColorTemperature
        {
            get => Internal_GetUseColorTemperature(m_InternalPtr);
            set => Internal_SetUseColorTemperature(m_InternalPtr, value);
        }

        /// <summary>Black-body color temperature in kelvin, clamped to 1000-40000 K.</summary>
        public float Temperature
        {
            get => Internal_GetTemperature(m_InternalPtr);
            set => Internal_SetTemperature(m_InternalPtr, value);
        }

        public uint VisibilityLayers
        {
            get => Internal_GetVisibilityLayers(m_InternalPtr);
            set => Internal_SetVisibilityLayers(m_InternalPtr, value);
        }

        public bool Enabled
        {
            get => Internal_GetEnabled(m_InternalPtr);
            set => Internal_SetEnabled(m_InternalPtr, value);
        }

        public bool AffectDiffuse
        {
            get => Internal_GetAffectDiffuse(m_InternalPtr);
            set => Internal_SetAffectDiffuse(m_InternalPtr, value);
        }

        public bool AffectSpecular
        {
            get => Internal_GetAffectSpecular(m_InternalPtr);
            set => Internal_SetAffectSpecular(m_InternalPtr, value);
        }

        public bool Volumetric
        {
            get => Internal_GetVolumetric(m_InternalPtr);
            set => Internal_SetVolumetric(m_InternalPtr, value);
        }

        public LightShadowMode Shadows
        {
            get => Internal_GetShadows(m_InternalPtr);
            set => Internal_SetShadows(m_InternalPtr, value);
        }

        public float ShadowBias
        {
            get => Internal_GetShadowBias(m_InternalPtr);
            set => Internal_SetShadowBias(m_InternalPtr, value);
        }

        public float ShadowNormalBias
        {
            get => Internal_GetShadowNormalBias(m_InternalPtr);
            set => Internal_SetShadowNormalBias(m_InternalPtr, value);
        }

        public float ShadowNearPlane
        {
            get => Internal_GetShadowNearPlane(m_InternalPtr);
            set => Internal_SetShadowNearPlane(m_InternalPtr, value);
        }

        public float ShadowImportance
        {
            get => Internal_GetShadowImportance(m_InternalPtr);
            set => Internal_SetShadowImportance(m_InternalPtr, value);
        }

        public uint ShadowResolution
        {
            get => Internal_GetShadowResolution(m_InternalPtr);
            set => Internal_SetShadowResolution(m_InternalPtr, value);
        }

        public bool CacheStaticShadowCasters
        {
            get => Internal_GetCacheStaticShadowCasters(m_InternalPtr);
            set => Internal_SetCacheStaticShadowCasters(m_InternalPtr, value);
        }

        [Obsolete("Use Type instead.")] public LightType type { get => Type; set => Type = value; }
        [Obsolete("Use Color instead.")] public Color color { get => Color; set => Color = value; }
        [Obsolete("Use Intensity instead.")] public float intensity { get => Intensity; set => Intensity = value; }
        [Obsolete("Use Range instead.")] public float range { get => Range; set => Range = value; }
        [Obsolete("Use SpotInnerAngle instead.")] public float spotInnerAngle { get => SpotInnerAngle; set => SpotInnerAngle = value; }
        [Obsolete("Use SpotOuterAngle instead.")] public float spotOuterAngle { get => SpotOuterAngle; set => SpotOuterAngle = value; }
        [Obsolete("Use Enabled instead.")] public bool enabled { get => Enabled; set => Enabled = value; }
        [Obsolete("Use Shadows instead.")] public LightShadowMode shadows { get => Shadows; set => Shadows = value; }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern LightType Internal_GetType(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetType(IntPtr thisPtr, LightType value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetColor(IntPtr thisPtr, out Color value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetColor(IntPtr thisPtr, ref Color value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetIntensity(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetIntensity(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRange(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRange(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetSpotInnerAngle(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSpotInnerAngle(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetSpotOuterAngle(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSpotOuterAngle(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetSourceRadius(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSourceRadius(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetUseColorTemperature(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetUseColorTemperature(IntPtr thisPtr, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetTemperature(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetTemperature(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetVisibilityLayers(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetVisibilityLayers(IntPtr thisPtr, uint value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetEnabled(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetEnabled(IntPtr thisPtr, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetAffectDiffuse(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAffectDiffuse(IntPtr thisPtr, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetAffectSpecular(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAffectSpecular(IntPtr thisPtr, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetVolumetric(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetVolumetric(IntPtr thisPtr, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern LightShadowMode Internal_GetShadows(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadows(IntPtr thisPtr, LightShadowMode value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetShadowBias(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadowBias(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetShadowNormalBias(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadowNormalBias(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetShadowNearPlane(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadowNearPlane(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetShadowImportance(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadowImportance(IntPtr thisPtr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetShadowResolution(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetShadowResolution(IntPtr thisPtr, uint value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetCacheStaticShadowCasters(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetCacheStaticShadowCasters(IntPtr thisPtr, bool value);
    }
}
