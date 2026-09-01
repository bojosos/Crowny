using System;

namespace Crowny
{
    /// <summary>
    /// Controls horizontal text alignment in inspector controls.
    /// </summary>
    public enum TextAlignment
    {
        /// <summary>Aligns the value label to the left edge.</summary>
        Left,

        /// <summary>Centers the value label.</summary>
        Center,

        /// <summary>Aligns the value label to the right edge.</summary>
        Right
    }

    /// <summary>
    /// Draws a numeric field or property as an editable horizontal progress bar in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public sealed class ProgressBar : Attribute
    {
        /// <summary>
        /// Creates a progress bar with fixed minimum and maximum values.
        /// </summary>
        public ProgressBar(double min, double max, float r = 0.15f, float g = 0.47f, float b = 0.74f)
        {
            Min = min;
            Max = max;
            R = r;
            G = g;
            B = b;
        }

        /// <summary>
        /// Creates a progress bar with a fixed minimum and a maximum read from a sibling numeric member.
        /// </summary>
        public ProgressBar(double min, string maxGetter, float r = 0.15f, float g = 0.47f, float b = 0.74f)
            : this(min, 0.0, r, g, b)
        {
            MaxGetter = maxGetter;
        }

        /// <summary>
        /// Creates a progress bar with a minimum read from a sibling numeric member and a fixed maximum.
        /// </summary>
        public ProgressBar(string minGetter, double max, float r = 0.15f, float g = 0.47f, float b = 0.74f)
            : this(0.0, max, r, g, b)
        {
            MinGetter = minGetter;
        }

        /// <summary>
        /// Creates a progress bar whose bounds are read from sibling numeric members.
        /// </summary>
        public ProgressBar(string minGetter, string maxGetter, float r = 0.15f, float g = 0.47f, float b = 0.74f)
            : this(0.0, 0.0, r, g, b)
        {
            MinGetter = minGetter;
            MaxGetter = maxGetter;
        }

        /// <summary>Gets the fixed minimum value.</summary>
        public double Min { get; private set; }

        /// <summary>Gets the fixed maximum value.</summary>
        public double Max { get; private set; }

        /// <summary>Gets the name of the sibling member that supplies the minimum value.</summary>
        public string MinGetter { get; private set; }

        /// <summary>Gets the name of the sibling member that supplies the maximum value.</summary>
        public string MaxGetter { get; private set; }

        /// <summary>Gets the red component of the bar color.</summary>
        public float R { get; private set; }

        /// <summary>Gets the green component of the bar color.</summary>
        public float G { get; private set; }

        /// <summary>Gets the blue component of the bar color.</summary>
        public float B { get; private set; }

        /// <summary>Gets or sets the bar height in pixels.</summary>
        public int Height { get; set; } = 12;

        /// <summary>Gets or sets whether the filled portion is divided into discrete segments.</summary>
        public bool Segmented { get; set; }

        /// <summary>Gets or sets whether the current value is drawn on top of the bar.</summary>
        public bool DrawValueLabel { get; set; } = true;

        /// <summary>Gets or sets the alignment of the value label.</summary>
        public TextAlignment ValueLabelAlignment { get; set; } = TextAlignment.Center;

        /// <summary>Gets or sets the name of a sibling color member that supplies the fill color.</summary>
        public string ColorGetter { get; set; }

        /// <summary>Gets or sets the name of a sibling color member that supplies the background color.</summary>
        public string BackgroundColorGetter { get; set; }

        /// <summary>Gets or sets the name of a sibling string member that supplies the displayed value label.</summary>
        public string CustomValueStringGetter { get; set; }
    }
}
