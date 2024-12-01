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
        public const float PI = 3.14159265359f;

        /// <summary>
        /// A constant that converts from degrees to radians.
        /// </summary>
        public const float Deg2Rad = 3.14159265359f / 180.0f;

        /// <summary>
        /// A constant that converts from radians to degrees.
        /// </summary>
        public const float Rad2Deg = 180.0f / 3.14159265359f;

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
        /// Returns the next(larger than the number) closest power of two.
        /// </summary>
        /// <param name="value">The number.</param>
        /// <returns>The next closest power of two or the number itself it it's a power of two.</returns>
        public static int NextPowerOfTwo(int value)
        {
            value -= 1;
            value |= value >> 16;
            value |= value >> 8;
            value |= value >> 4;
            value |= value >> 2;
            value |= value >> 1;
            return value + 1;
        }

        /// <summary>
        /// Returns the largest power of two that is smaller than the number.
        /// </summary>
        /// <param name="value">The number.</param>
        /// <returns>The largest power of two that is smaller than the number.</returns>
        public static int ClosestPowerOfTwo(int value)
        {
            int nextPower = NextPowerOfTwo(value);
            int prevPower = nextPower >> 1;
            if (value - prevPower < nextPower - value)
                return prevPower;
            else
                return nextPower;
        }

        /// <summary>
        /// Checks if a number is a power of two.
        /// </summary>
        /// <param name="value">The number.</param>
        /// <returns>True if the number if is a power of two, false otherwise.</returns>
        public static bool IsPowerOfTwo(int value)
        {
            return (value & (value - 1)) == 0;
        }

        /// <summary>
        /// Returns the inverse square root 1/sqrt(x).
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Inverse square root of the provided parameter.</returns>
        public static float InvSqrt(float f)
        {
            return 1.0f / (float)Math.Sqrt(f);
        }

        /// <summary>
        /// Returns the square root.
        /// </summary>
        /// <param name="f">Parameter.</param>
        /// <returns>Square root of the provided parameter.</returns>
        public static float Sqrt(float f)
        {
            return 1.0f / (float)Math.Sqrt(f);
        }

        /// <summary>
        /// Rounds down f to the largest integer smaller than f.
        /// </summary>
        /// <param name="f">The number to round down.</param>
        /// <returns>The resulting floored number.</returns>
        public static float Floor(float f)
        {
            return (float)Math.Floor(f);
        }

        /// <summary>
        /// Clamps a number between two values.
        /// </summary>
        /// <param name="v">The value to clamp.</param>
        /// <param name="min">The min value of the nubmer.</param>
        /// <param name="max">The max value of the number.</param>
        /// <returns>The clamped value.</returns>
        public static float Clamp(float v, float min, float max)
        {
            if (v < min)
                v = min;
            else if (v > max)
                v = max;
            return v;
        }

        /// <summary>
        /// Clamps a number between two values.
        /// </summary>
        /// <param name="v">The value to clamp.</param>
        /// <param name="min">The min value of the nubmer.</param>
        /// <param name="max">The max value of the number.</param>
        /// <returns>The clamped value.</returns>
        public static int Clamp(int v, int min, int max)
        {
            if (v < min)
                v = min;
            else if (v > max)
                v = max;
            return v;
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
        /// e^power.
        /// </summary>
        /// <param name="power">The power to raise `e` to.</param>
        /// <returns>e^power.</returns>
        public static float Exp(float power) { return (float)Math.Exp(power); }

        /// <summary>
        /// 2^power.
        /// </summary>
        /// <param name="power">The power to raise 2 to.</param>
        /// <returns>2^power.</returns>
        public static float Exp2(float power) { return (float)Math.Pow(2, power); }

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

        /// <summary>
        /// Frame-independent lerp smoothing.
        /// </summary>
        /// <param name="a">The current value.</param>
        /// <param name="b">The target vlaue.</param>
        /// <param name="dt">Time.deltaTime</param>
        /// <param name="h">Time until halfway.</param>
        /// <returns>The interpolated value.</returns>
        public static float LerpSmooth(float a, float b, float dt, float h)
        {
            return b + (a - b) * Exp2(-dt / h);
        }

        public static float SmoothStep(float from, float to, float t)
        {
            t = Mathf.Clamp01(t);
            t = -2.0F * t * t * t + 3.0F * t * t;
            return to * t + from * (1F - t);
        }

        /// <summary>
        /// Pretty much an fmod function. Not defined for negative numbers.
        /// </summary>
        /// <param name="t">The value to to take the modulus from.</param>
        /// <param name="length">The mod value.</param>
        /// <returns>f%length</returns>
        public static float Repeat(float t, float length)
        {
            return Clamp(t - Floor(t / length) * length, 0.0f, length);
        }

        /// <summary>
        /// Linearly jumps between 0 and length and length back to 0(in a triangle wave pattern).
        /// </summary>
        /// <param name="t">The initial value. Has to be self incrementing e.g. Time.time.</param>
        /// <param name="length">The length of the interval to jump between.</param>
        /// <returns>The calculated value.</returns>
        public static float PingPing(float t, float length)
        {
            t = Repeat(t, length * 2f);
            return length - Abs(t - length);
        }
    }
}