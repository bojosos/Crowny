using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>
    /// A four dimensional vector.
    /// </summary>
    [StructLayout(LayoutKind.Sequential), SerializeObject]
    public struct Vector4
    {
        public static readonly Vector4 zero = new Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        public static readonly Vector4 one = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        public static readonly Vector4 right = new Vector4(1.0f, 0.0f, 0.0f, 0.0f);
        public static readonly Vector4 left = new Vector4(-1.0f, 0.0f, 0.0f, 0.0f);
        public static readonly Vector4 up = new Vector4(0.0f, 1.0f, 0.0f, 0.0f);
        public static readonly Vector4 down = new Vector4(0.0f, -1.0f, 0.0f, 0.0f);
        public static readonly Vector4 forward = new Vector4(0.0f, 0.0f, 1.0f, 0.0f);
        public static readonly Vector4 back = new Vector4(0.0f, 0.0f, -1.0f, 0.0f);

        public float x;
        public float y;
        public float z;
        public float w;

        public const float kEpsilon = 0.000001F;

        /// <summary>
        /// Access a component of the vector.
        /// </summary>
        /// <param name="index">Index: 0-x, 1-y, 2-z, 3-w.</param>
        /// <returns>The value of the component.</returns>
        public float this[int index]
        {
            get
            {
                switch(index)
                {
                    case 0: return x;
                    case 1: return y;
                    case 2: return z;
                    case 3: return w;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a four component Vector");
                }
            }
            set
            {
                switch (index)
                {
                    case 0: x = value; break;
                    case 1: y = value; break;
                    case 2: z = value; break;
                    case 3: w = value; break;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a four component Vector");

                }
            }
        }

        /// <summary>
        /// Returns a normalized copy of the Vector4.
        /// </summary>
        public Vector4 normalized
        {
            get
            {
                return Normalize(this);
            }
        }

        /// <summary>
        /// Normalizes the vector.
        /// </summary>
        public void Normalize()
        {
            float sl = this.sqrdLength;
            if (sl > 1e-04f)
                this *= 1.0f / (float)System.Math.Sqrt(sl);
        }

        /// <summary>
        /// Returns the length of the Vector4.
        /// </summary>
        public float length
        {
            get
            {
                return (float)System.Math.Sqrt(x * x + y * y + z * z + w * w);
            }
        }

        /// <summary>
        /// Returns the length of the Vector4 squared.
        /// </summary>
        public float sqrdLength
        {
            get
            {
                return (x * x + y * y + z * z + w * w);
            }
        }

        /// <summary>
        /// Creates a four component vector.
        /// </summary>
        /// <param name="x">X Coordinate.</param>
        /// <param name="y">Y Coordinate.</param>
        /// <param name="z">Z Coordinate.</param>
        public Vector4(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        /// <summary>
        /// Creates a four component vector.
        /// </summary>
        /// <param name="value">The value for all four components.</param>
        public Vector4(float value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
            this.w = value;
        }

        public static Vector4 operator+(Vector4 a, Vector4 b)
        {
            return new Vector4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
        }

        public static Vector4 operator-(Vector4 a, Vector4 b)
        {
            return new Vector4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
        }

        public static Vector4 operator*(Vector4 a, float b)
        {
            return new Vector4(a.x * b, a.y * b, a.z * b, a.w * b);
        }
        public static Vector4 operator*(Vector4 a, Vector4 b)
        {
            return new Vector4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
        }

        public static Vector4 operator/(Vector4 a, float b)
        {
            return new Vector4(a.x / b, a.y / b, a.z / b, a.w / b);
        }

        public static Vector4 operator /(Vector4 a, Vector4 b)
        {
            return new Vector4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
        }

        public static Vector4 operator-(Vector4 a)
        {
            return new Vector4(-a.x, -a.y, -a.z, -a.w);
        }

        public static bool operator==(Vector4 l, Vector4 r)
        {
            float dx = l.x - r.x;
            float dy = l.y - r.y;
            float dz = l.z - r.z;
            float dw = l.w - r.w;
            float sqrMag = dx * dx + dy * dy + dz * dz + dw * dw;
            return sqrMag < kEpsilon * kEpsilon;
        }

        public static bool operator!=(Vector4 l, Vector4 r)
        {
            return !(l == r);
        }

        /// <summary>
        /// Calculates the magnitude of a Vector4.
        /// </summary>
        /// <param name="v">The Vector4 to calculate the magnited of.</param>
        /// <returns>The magnitude of the vector.</returns>
        public static float Magnitude(Vector4 v)
        {
            return Mathf.Sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
        }

        /// <summary>
        /// Normalizes the vector.
        /// </summary>
        /// <param name="v">Vector.</param>
        /// <returns>Normalized copy of the vector.</returns>
        public static Vector4 Normalize(Vector4 v)
        {
            float sl = v.sqrdLength;
            if (sl > 1e-04f)
                return v * (float)Mathf.InvSqrt(sl);
            return v;
        }

        /// <summary>
        /// Calculates the distance between two points.
        /// </summary>
        /// <param name="a">First two dimensional point.</param>
        /// <param name="b">First two dimensional point.</param>
        /// <returns>Distance between the points</returns>
        public static float Distance(Vector4 a, Vector4 b)
        {
            Vector4 vec = new Vector4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
            return Mathf.Sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
        }

        /// <summary>
        /// Returns the maximum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the maximum components of the two vectors.</returns>
        public static Vector4 Max(Vector4 a, Vector4 b)
        {
            return new Vector4(Mathf.Max(a.x, b.x), Mathf.Max(a.y, b.y), Mathf.Max(a.z, b.z), Mathf.Max(a.w, b.w));
        }

        /// <summary>
        /// Returns the minimum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the minimum components of the two vectors.</returns>
        public static Vector4 Min(Vector4 a, Vector4 b)
        {
            return new Vector4(Mathf.Min(a.x, b.x), Mathf.Min(a.y, b.y), Mathf.Min(a.z, b.z), Mathf.Min(a.w, b.w));
        }

        /// <summary>
        /// Linearly interpolates between two vector3s.
        /// </summary>
        /// <param name="a">Starting vector.</param>
        /// <param name="b">Ending vector.</param>
        /// <param name="t">Interpolation factor.</param>
        /// <returns>Interpolated vector</returns>
        public static Vector4 Lerp(Vector3 a, Vector3 b, float t)
        {
            t = Mathf.Clamp01(t);
            return new Vector4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
        }

        /// <inheritdoc/>
        public override string ToString()
        {
            return "(" + x + ", " + y + ", " + z + ", " + w + ")";
        }

         /// <inheritdoc/>
        public override int GetHashCode()
        {
            return x.GetHashCode() ^ y.GetHashCode() << 2 ^ z.GetHashCode() >> 2 ^ w.GetHashCode() >> 1;
        }

        /// <inheritdoc/>
        public override bool Equals(object other)
        {
            if (!(other is Vector4))
                return false;
            Vector4 v = (Vector4)other;
            return x.Equals(v.x) && y.Equals(v.y) && z.Equals(v.z) && w.Equals(v.w);
        }
    }
}
