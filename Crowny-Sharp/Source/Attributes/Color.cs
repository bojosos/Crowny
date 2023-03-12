using System;

namespace Crowny
{
    /// <summary>
    /// ColorUsage can be used to control how color fields are displayed.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class ColorUsage : Attribute
    {
#pragma warning disable 0414
        private bool hdr;
        private bool showAlpha;
#pragma warning restore 0414

        /// <summary>
        /// Creates a ColorUsage attribute.
        /// </summary>
        /// <param name="showAlpha">If false then the alpha channel is hidden in color picker and color palettes.</param>
        public ColorUsage(bool showAlpha)
        {
            this.showAlpha = showAlpha;
            this.hdr = false;
        }

        /// <summary>
        /// Creates a ColorUsage attribute.
        /// </summary>
        /// <param name="hdr">If set to true if the color should be displayed with hdr.</param>
        /// <param name="showAlpha">If false then the alpha channel is hidden in color picker and color palettes.</param>
        public ColorUsage(bool hdr, bool showAlpha)
        {
            this.hdr = hdr;
            this.showAlpha = showAlpha;
        }
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