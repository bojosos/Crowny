using System;

namespace Crowny
{

    /// <summary>
    /// Makes a field displayed as a slider in the inspector.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public class Range : Attribute
    {
        #pragma warning disable 0414
        private float min;
        private float max;
        private bool slider;
        #pragma warning restore 0414

        /// <summary>
        /// Creates a range attribute.
        /// </summary>
        /// <param name="min">Minimum value of the range.</param>
        /// <param name="max">Maximum value of the range.</param>
        /// <param name="slider">WHether it should be rendered as a slider.</param>
        public Range(float min, float max, bool slider = true)
        {
            this.min = min;
            this.max = max;
            this.slider = slider;
        }
    }
}