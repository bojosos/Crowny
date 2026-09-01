using System;

namespace Crowny
{
    /// <summary>
    /// Draws a string field or property as a multiline text box.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class Multiline : Attribute
    {
        /// <summary>Creates a multiline text box with the requested visible line count.</summary>
        /// <param name="lines">The number of visible text lines.</param>
        public Multiline(int lines = 3)
        {
            Lines = lines;
        }

        /// <summary>Gets or sets the number of visible text lines.</summary>
        public int Lines { get; set; }
    }

    /// <summary>
    /// Odin-compatible spelling for <see cref="Multiline"/>.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public sealed class MultiLineProperty : Multiline
    {
        /// <summary>Creates a multiline text box with the requested visible line count.</summary>
        /// <param name="lines">The number of visible text lines.</param>
        public MultiLineProperty(int lines = 3) : base(lines) {}
    }
}
