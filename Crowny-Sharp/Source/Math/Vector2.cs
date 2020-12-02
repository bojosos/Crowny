using System;
using System.Runtime.InteropServices;

namespace Crowny
{

    /// <summary>
    /// A two dimensional vector.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public static readonly Vector2 zero = new Vector2(0.0f, 0.0f);
        public static readonly Vector2 one = new Vector2(1.0f, 1.0f);
        public static readonly Vector2 right = new Vector2(1.0f, 0.0f);
        public static readonly Vector2 left = new Vector2(-1.0f, 0.0f);
        public static readonly Vector2 up = new Vector2(0.0f, 1.0f);
        public static readonly Vector2 down = new Vector2(0.0f, -1.0f);

        public float x;
        public float y;
        
        /// <summary>
        /// Creates a new Vector2.
        /// </summary>
        /// <param name="x">X Coordinate.</param>
        /// <param name="y">Y Coordinate.</param>
        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        /// <summary>
        /// Creates a new Vector2.
        /// </summary>
        /// <param name="value">The value for all three components.</param>
        public Vector2(float value)
        {
            this.x = value;
            this.y = value;
        }

        /// <summary>
        /// Access a component of the vector.
        /// </summary>
        /// <param name="index">Index of the component.</param>
        /// <returns>The value of the component.</returns>
        public float this[int index]
        {
            get
            {
                switch(index)
                {
                    case (0): return x;
                    case (1): return y;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a three component Vector");
                }
            }
            set
            {
                switch (index)
                {
                    case (0): x = value; break;
                    case (1): y = value; break;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a three component Vector");

                }
            }
        }

        /// <summary>
        /// Returns a normalized copy of the Vector2.
        /// </summary>
        public Vector2 normalized
        {
            get
            {
                return Normalize(this);
            }
        }

        /// <summary>
        /// Returns the length of the Vector2.
        /// </summary>
        public float length
        {
            get
            {
                return (float)System.Math.Sqrt(x * x + y * y );
            }
        }

        /// <summary>
        /// Returns the length of the Vector2 squared.
        /// </summary>
        public float sqrdLength
        {
            get
            {
                return (x * x + y * y);
            }
        }

        /// <summary>
        /// Normalizes the Vector2.
        /// </summary>
        public void Normalize()
        {
            float sl = this.sqrdLength;
            if (sl > 1e-04f)
                this *= 1.0f / (float)System.Math.Sqrt(sl);
        }

        /// <summary>
        /// Scales the components of the vector
        /// </summary>
        /// <param name="scale">Scale factors to multiply components by</param>
        public void Scale(Vector2 scale)
        {
            x *= scale.x;
            y *= scale.y;
        }

        public static Vector2 operator+(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x + b.x, a.y + b.y);
        }

        public static Vector2 operator-(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x - b.x, a.y - b.y);
        }

        public static Vector2 operator*(Vector2 a, float b)
        {
            return new Vector2(a.x * b, a.y * b);
        }
        public static Vector2 operator*(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x * b.x, a.y * b.y);
        }

        public static Vector2 operator/(Vector2 a, float b)
        {
            return new Vector2(a.x / b, a.y / b);
        }

        public static Vector2 operator /(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x / b.x, a.y / b.y);
        }

        public static Vector2 operator-(Vector2 a)
        {
            return new Vector2(-a.x, -a.y);
        }

        public static bool operator==(Vector2 l, Vector2 r)
        {
            return l.x == r.x && l.y == r.y;
        }

        public static bool operator!=(Vector2 l, Vector2 r)
        {
            return l != r;
        }

        /// <summary>
        /// Calculates the magniuted of a Vector2
        /// </summary>
        /// <param name="v">The Vector2 to calculate the magnited of</param>
        /// <returns>The magnitude of the vector</returns>
        public static float Magnitude(Vector2 v)
        {
            return (float)System.Math.Sqrt(v.x * v.x + v.y * v.y);
        }

        /// <summary>
        /// Normalizes the vector
        /// </summary>
        /// <param name="v">Vector</param>
        /// <returns>Normalized copy of the vector</returns>
        public static Vector2 Normalize(Vector2 v)
        {
            float sl = v.sqrdLength;
            if (sl > 1e-04f)
                return v * Mathf.InvSqrt(sl);
            return v;
        }

        /// <summary>
        /// Calculates the dinstance between two points.
        /// </summary>
        /// <param name="a">First two dimensional point.</param>
        /// <param name="b">First two dimensional point.</param>
        /// <returns>Distance between the points</returns>
        public static float Distance(Vector2 a, Vector2 b)
        {
            Vector2 vec = new Vector2(a.x - b.x, a.y - b.y);
            return Mathf.Sqrt(vec.x * vec.x + vec.y * vec.y);
        }

        /// <summary>
        /// Returns the maximum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the maximum components of the two vectors</returns>
        public static Vector2 Max(Vector2 a, Vector2 b)
        {
            return new Vector2(Mathf.Max(a.x, b.x), Mathf.Max(a.y, b.y));
        }

        /// <summary>
        /// Returns the minumum of all the vector components as a new vector.
        /// </summary>
        /// <param name="a">First vector.</param>
        /// <param name="b">Second vector.</param>
        /// <returns>Vector with the minumum components of the two vectors</returns>
        public static Vector2 Min(Vector2 a, Vector2 b)
        {
            return new Vector2(Mathf.Min(a.x, b.x), Mathf.Min(a.y, b.y));
        }

         /// <inheritdoc/>
        public override int GetHashCode()
        {
            return x.GetHashCode() ^ y.GetHashCode() << 2;
        }

        /// <inheritdoc/>
        public override string ToString()
        {
            return "(" + x + ", " + y + ")";
        }

        /// <inheritdoc/>
        public override bool Equals(object other)
        {
            if (!(other is Vector2))
                return false;
            Vector2 v = (Vector2)other;
            return x.Equals(v.x) && y.Equals(v.y);
        }
    }
}
