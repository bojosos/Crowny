using System;

namespace Crowny
{

    /// <summary>
    /// Used to specify the minimum and maximum values for a numeric field. This attribute can also be used for List/Array that contain numeric elements.
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
        /// <param name="slider">Whether it should be rendered as a slider.</param>
        public Range(float min, float max, bool slider = true)
        {
            this.min = min;
            this.max = max;
            this.slider = slider;
        }
    }
}