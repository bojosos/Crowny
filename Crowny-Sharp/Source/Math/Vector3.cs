using System;
using System.Runtime.InteropServices;

namespace Crowny.Math
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public static readonly Vector3 Zero = new Vector3(0.0f, 0.0f, 0.0f);
        public static readonly Vector3 One = new Vector3(1.0f, 1.0f, 1.0f);
        public static readonly Vector3 Up = new Vector3(0.0f, 1.0f, 0.0f);
        public static readonly Vector3 Right = new Vector3(1.0f, 0.0f, 0.0f);
        public static readonly Vector3 Forward = new Vector3(0.0f, 0.0f, 1.0f);

        public float x;
        public float y;
        public float z;

        /// <summary>
        /// Access a component of the vector
        /// </summary>
        /// <param name="index">Index: 0-x, 1-y, 2-z</param>
        /// <returns>The value of the component</returns>
        public float this[int index]
        {
            get
            {
                switch(index)
                {
                    case (0): return x;
                    case (1): return y;
                    case (2): return z;
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
                    case (2): z = value; break;
                    default:
                        throw new IndexOutOfRangeException("Invalid index for a three component Vector");

                }
            }
        }

        public override string ToString()
        {
            return x.ToString() + ", " + y.ToString() + ", " + z.ToString();
        }

        /// <summary>
        /// Returns a normalized copy of the Vector3
        /// </summary>
        public Vector3 Normalized
        {
            get
            {
                return Normalize(this);
            }
        }

        /// <summary>
        /// Normalied the Vector3
        /// </summary>
        public void Normalize()
        {
            float sl = this.SqrdLength;
            if (sl > 1e-04f)
                this *= 1.0f / (float)System.Math.Sqrt(sl);
        }

        /// <summary>
        /// Returns the length of the Vector3
        /// </summary>
        public float Length
        {
            get
            {
                return (float)System.Math.Sqrt(x * x + y * y + z * z);
            }
        }

        /// <summary>
        /// Returns the length of the Vector3 squared
        /// </summary>
        public float SqrdLength
        {
            get
            {
                return (x * x + y * y + z * z);
            }
        }

        /// <summary>
        /// Creates a Vector3
        /// </summary>
        /// <param name="x">X Coordinate</param>
        /// <param name="y">Y Coordinate</param>
        /// <param name="z">Z Coordinate</param>
        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        /// <summary>
        /// Creates a Vector3
        /// </summary>
        /// <param name="value">The value for all three components</param>
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
            return l != r;
        }

        /// <summary>
        /// Calculates the magniuted of a Vector3
        /// </summary>
        /// <param name="v">The Vector3 to calculate the magnited of</param>
        /// <returns>The magnitude of the vector</returns>
        public static float Magnitude(Vector3 v)
        {
            return (float)System.Math.Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        public static Vector3 Normalize(Vector3 v)
        {
            float sl = v.SqrdLength;
            if (sl > 1e-04f)
                return v * 1.0f / (float)System.Math.Sqrt(sl);
            return v;
        }
    }
}
