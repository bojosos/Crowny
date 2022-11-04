using System;

namespace Crowny
{
    /// <summary>
    /// Class providing common math operations.
    /// </summary>
    public class Mathf
    {
        /// <summary>
        /// Pi
        /// </summary>
        public static readonly float Pi = 3.14159265359f;

        /// <summary>
        /// A constant that converts from degrees to radians.
        /// </summary>
        public static readonly float Deg2Rad = 3.14159265359f / 180.0f;

        /// <summary>
        /// A constant that converts from radians to degrees.
        /// </summary>
        public static readonly float Rad2Deg = 180.0f / 3.14159265359f;

        /// <summary>
        /// Returns the minimum value.
        /// </summary>
        /// <param name="a">First value.</param>
        /// <param name="b">Second value.</param>
        /// <returns>Minimum of the two values</returns>
        public static float Min(float a, float b)
        {
            if (a < b)
                return a;
            return b;
        }

        /// <summary>
        /// Returns the minimum of the provided values.
        /// </summary>
        /// <param name="values">Values.</param>
        /// <returns>Minimum of the values.</returns>
        public static float Min(params float[] values)
        {
            int length = values.Length;
            if (length == 0)
                return 0.0f;
            float min = values[0];
            for (int i = 1; i < length; i++)
            {
                if (values[i] < min)
                    min = values[i];
            }

            return min;
        }

        /// <summary>
        /// Returns the minimum value.
        /// </summary>
        /// <param name="a">First value.</param>
        /// <param name="b">Second value.</param>
        /// <returns>Minimum of the two values</returns>
        public static int Min(int a, int b)
        {
            if (a < b)
                return a;
            return b;
        }

        /// <summary>
        /// Returns the minimum of the provided values.
        /// </summary>
        /// <param name="values">Values.</param>
        /// <returns>Minimum of the values.</returns>
        public static int Min(params int[] values)
        {
            int length = values.Length;
            if (length == 0)
                return 0;
            int min = values[0];
            for (int i = 1; i < length; i++)
            {
                if (values[i] < min)
                    min = values[i];
            }

            return min;
        }

        /// <summary>
        /// Returns the maximum value.
        /// </summary>
        /// <param name="a">First value.</param>
        /// <param name="b">Second value.</param>
        /// <returns>Maximum of the two values</returns>
        public static float Max(float a, float b)
        {
            if (a > b)
                return a;
            return b;
        }

        /// <summary>
        /// Returns the maximum of the provided values.
        /// </summary>
        /// <param name="values">Values.</param>
        /// <returns>Maximum of the values.</returns>
        public static float Max(params float[] values)
        {
            int length = values.Length;
            if (length == 0)
                return 0.0f;
            float max = values[0];
            for (int i = 1; i < length; i++)
            {
                if (values[i] > max)
                    max = values[i];
            }

            return max;
        }

        /// <summary>
        /// Returns the maximum value.
        /// </summary>
        /// <param name="a">First value.</param>
        /// <param name="b">Second value.</param>
        /// <returns>Maximum of the two values</returns>
        public static int Max(int a, int b)
        {
            if (a > b)
                return a;
            return b;
        }

        /// <summary>
        /// Returns the maximum of the provided values.
        /// </summary>
        /// <param name="values">Values.</param>
        /// <returns>Maximum of the values.</returns>
        public static int Max(params int[] values)
        {
            int length = values.Length;
            if (length == 0)
                return 0;
            int max = values[0];
            for (int i = 1; i < length; i++)
            {
                if (values[i] > max)
                    max = values[i];
            }

            return max;
        }

        /// <summary>
        /// Returns the absolute value.
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>The abolute value of <paramref name="f"/>.</returns>
        public static float Abs(float f)
        {
            return Math.Abs(f);
        }

        /// <summary>
        /// Returns the absolute value.
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>The abolute value of <paramref name="f"/>.</returns>
        public static int Abs(int f)
        {
            return Math.Abs(f);
        }

        /// <summary>
        /// Raises <paramref name="f"/> to the power of <paramref name="exp"/>
        /// </summary>
        /// <param name="f">Value to raise.</param>
        /// <param name="exp">Power to raise to.</param>
        /// <returns><paramref name="f"/> raised to the power of <paramref name="exp"/>.</returns>
        public static float Pow(float f, float exp)
        {
            return (float)Math.Pow(f, exp);
        }

        /// <summary>
        /// Returns the natural logarithm of <paramref name="f"/>
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Natural logarithm of <paramref name="f"/>.</returns>
        public static float Log(float f)
        {
            return (float)Math.Log(f);
        }

        /// <summary>
        /// Returns the logarithm of <paramref name="f"/> in base 10.
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Natural logarithm <paramref name="f"/>.</returns>
        public static float Log10(float f)
        {
            return (float)Math.Log10(f);
        }

        /// <summary>
        /// Returns the inverse square root 1/sqrt(x).
        /// /// /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Inverse square root of the provided parameter.</returns>
        public static float InvSqrt(float f)
        {
            return 1.0f / (float)Math.Sqrt(f);
        }

        /// <summary>
        /// Returns the square root.
        /// /// /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Square root of the provided parameter.</returns>
        public static float Sqrt(float f)
        {
            return 1.0f / (float)Math.Sqrt(f);
        }

        /// <summary>
        /// Clamps a value between 0 and 1
        /// </summary>
        /// <param name="v">Paramter.</param>
        /// <returns><paramref name="v"> if it is in range [0, 1], otherwise returns value clamped to the range.</returns>
        public static float Clamp01(float v)
        {
            if (v < 0.0f)
                return 0.0f;

            if (v > 1.0f)
                return 1.0f;

            return v;
        }

        /// <summary>
        /// Linearly interpolates between two values.
        /// </summary>
        /// <param name="a">Starting value.</param>
        /// <param name="b">Ending value.</param>
        /// <param name="t">Interpolation factor.</param>
        /// <param name="tmin">Minimum value for the range.</param>
        /// <param name="tmax">Maximum value for the range.</param>
        /// <returns>Interpolated value</returns>
        public static float Lerp(float a, float b, float t, float tmin = 0.0f, float tmax = 1.0f)
        {
            t = Clamp01((t - tmin) / (tmax - tmin));
            return a * (1.0f - t) + b * t;
        }
    }
}