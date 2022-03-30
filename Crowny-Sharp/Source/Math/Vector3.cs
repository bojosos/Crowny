using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>
    /// A three dimensional vector.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    // TODO: Maybe IEquatable, IFormattable, for all classes
    public struct Vector3
    {
        public static readonly Vector3 zero = new Vector3(0.0f, 0.0f, 0.0f);
        public static readonly Vector3 one = new Vector3(1.0f, 1.0f, 1.0f);
        public static readonly Vector3 right = new Vector3(1.0f, 0.0f, 0.0f);
        public static readonly Vector3 left = new Vector3(-1.0f, 0.0f, 0.0f);
        public static readonly Vector3 up = new Vector3(0.0f, 1.0f, 0.0f);
        public static readonly Vector3 down = new Vector3(0.0f, -1.0f, 0.0f);
        public static readonly Vector3 forward = new Vector3(0.0f, 0.0f, 1.0f);
        public static readonly Vector3 back = new Vector3(0.0f, 0.0f, -1.0f);

        public float x;
        public float y;
        public float z;

        /// <summary>
        /// Access a component of the vector.
        /// </summary>
        /// <param name="index">Index: 0-x, 1-y, 2-z.</param>
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
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a three component Vector");
                }
            }
            set
            {
                switch (index)
                {
                    case 0: x = value; break;
                    case 1: y = value; break;
                    case 2: z = value; break;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a three component Vector");

                }
            }
        }

        /// <summary>
        /// Returns a normalized copy of the Vector3.
        /// </summary>
        public Vector3 normalized
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
        /// Returns the length of the Vector3.
        /// </summary>
        public float length
        {
            get
            {
                return (float)System.Math.Sqrt(x * x + y * y + z * z);
            }
        }

        /// <summary>
        /// Returns the length of the Vector3 squared.
        /// </summary>
        public float sqrdLength
        {
            get
            {
                return (x * x + y * y + z * z);
            }
        }

        /// <summary>
        /// Creates a Vector3.
        /// </summary>
        /// <param name="x">X Coordinate.</param>
        /// <param name="y">Y Coordinate.</param>
        /// <param name="z">Z Coordinate.</param>
        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        /// <summary>
        /// Creates a Vector3.
        /// </summary>
        /// <param name="value">The value for all three components.</param>
        public Vector3(float value)
        {
            this.x = value;
            this.y = value;
            this.z = value;
        }

        public static Vector3 operator+(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        }

        public static Vector3 operator-(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        }

        public static Vector3 operator*(Vector3 a, float b)
        {
            return new Vector3(a.x * b, a.y * b, a.z * b);
        }
        public static Vector3 operator*(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
        }

        public static Vector3 operator/(Vector3 a, float b)
        {
            return new Vector3(a.x / b, a.y / b, a.z / b);
        }

        public static Vector3 operator /(Vector3 a, Vector3 b)
        {
            return new Vector3(a.x / b.x, a.y / b.y, a.z / b.z);
        }

        public static Vector3 operator-(Vector3 a)
        {
            return new Vector3(-a.x, -a.y, -a.z);
        }

        public static bool operator==(Vector3 l, Vector3 r)
        {
            return l.x == r.x && l.y == r.y && l.z == r.z;
        }

        public static bool operator!=(Vector3 l, Vector3 r)
        {
            return !(l == r);
        }

        /// <summary>
        /// Calculates the magniuted of a Vector3.
        /// </summary>
        /// <param name="v">The Vector3 to calculate the magnited of.</param>
        /// <returns>The magnitude of the vector.</returns>
        public static float Magnitude(Vector3 v)
        {
            return (float)System.Math.Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        /// <summary>
        /// Normalizes the vector.
        /// </summary>
        /// <param name="v">Vector.</param>
        /// <returns>Normalized copy of the vector.</returns>
        public static Vector3 Normalize(Vector3 v)
        {
            float sl = v.sqrdLength;
            if (sl > 1e-04f)
                return v * (float)Mathf.InvSqrt(sl);
            return v;
        }

        /// <summary>
        /// Calculates the dinstance between two points.
        /// </summary>
        /// <param name="a">First two dimensional point.</param>
        /// <param name="b">First two dimensional point.</param>
        /// <returns>Distance between the points</returns>
        public static float Distance(Vector3 a, Vector3 b)
        {
            Vector3 vec = new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
            return Mathf.Sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        }

        /// <summary>
        /// Returns the maximum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the maximum components of the two vectors</returns>
        public static Vector3 Max(Vector3 a, Vector3 b)
        {
            return new Vector3(Mathf.Max(a.x, b.x), Mathf.Max(a.y, b.y), Mathf.Max(a.z, b.z));
        }

        /// <summary>
        /// Returns the minumum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the minumum components of the two vectors</returns>
        public static Vector3 Min(Vector3 a, Vector3 b)
        {
            return new Vector3(Mathf.Min(a.x, b.x), Mathf.Min(a.y, b.y), Mathf.Min(a.z, b.z));
        }

        /// <inheritdoc/>
        public override string ToString()
        {
            return "(" + x + ", " + y + ", " + z + ")";
        }

         /// <inheritdoc/>
        public override int GetHashCode()
        {
            return x.GetHashCode() ^ y.GetHashCode() << 2 ^ z.GetHashCode() >> 2;
        }

        /// <inheritdoc/>
        public override bool Equals(object obj)
        {
            if (!(obj is Vector3))
                return false;
            Vector3 v = (Vector3)obj;
            return x.Equals(v.x) && y.Equals(v.y) && z.Equals(v.z);
        }
    }
}
