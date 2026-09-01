using System;

namespace Crowny
{
    /// <summary>
    /// ColorUsage can be used to control how color fields are displayed.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true, AllowMultiple = false)]
    public class ColorUsage : Attribute
    {
        /// <summary>
        /// Creates a ColorUsage attribute.
        /// </summary>
        /// <param name="showAlpha">If false then the alpha channel is hidden in color picker and color palettes.</param>
        public ColorUsage(bool showAlpha)
            : this(showAlpha, false)
        {
        }

        /// <summary>
        /// Creates a ColorUsage attribute.
        /// </summary>
        /// <param name="showAlpha">If false then the alpha channel is hidden in color picker and color palettes.</param>
        /// <param name="hdr">If true the color picker accepts HDR component values.</param>
        public ColorUsage(bool showAlpha, bool hdr)
        {
            this.showAlpha = showAlpha;
            this.hdr = hdr;
        }

        /// <summary>
        /// Creates a ColorUsage attribute with the legacy HDR picker limits. Unity no longer uses the four limit values.
        /// </summary>
        [Obsolete("Brightness and exposure parameters are no longer used. Use ColorUsage(bool showAlpha, bool hdr) instead.")]
        public ColorUsage(bool showAlpha, bool hdr, float minBrightness, float maxBrightness, float minExposureValue, float maxExposureValue)
            : this(showAlpha, hdr)
        {
            this.minBrightness = minBrightness;
            this.maxBrightness = maxBrightness;
            this.minExposureValue = minExposureValue;
            this.maxExposureValue = maxExposureValue;
        }

        /// <summary>Gets whether the inspector shows and edits the alpha channel.</summary>
        public bool showAlpha { get; private set; }

        /// <summary>Gets whether the inspector treats the color as HDR.</summary>
        public bool hdr { get; private set; }

        [Obsolete("This property is no longer used.")]
        public float minBrightness { get; private set; }

        [Obsolete("This property is no longer used.")]
        public float maxBrightness { get; private set; }

        [Obsolete("This property is no longer used.")]
        public float minExposureValue { get; private set; }

        [Obsolete("This property is no longer used.")]
        public float maxExposureValue { get; private set; }
    }

    /// <summary>
    /// ColorPalette can be used to specify a default color palette for a color field.
    /// Color palettes can be edited in the color field or in the settings.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class ColorPalette : Attribute
    {
#pragma warning disable 0414
        private string paletteName;
#pragma warning restore 0414

        public ColorPalette(string paletteName)
        {
            this.paletteName = paletteName;
        }
    }
}
