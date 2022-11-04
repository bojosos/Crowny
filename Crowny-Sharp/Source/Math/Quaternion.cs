using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>
    /// A quaternion.
    /// </summary>
    [StructLayout(LayoutKind.Sequential), SerializeObject]
    public struct Quaternion
    {
        /// <summary>
        /// X component of the quaternion. Do not modify unless you know what you are doing.
        /// </summary>
        public float x;
        /// <summary>
        /// Y component of the quaternion. Do not modify unless you know what you are doing.
        /// </summary>
        public float y;
        /// <summary>
        /// Z component of the quaternion. Do not modify unless you know what you are doing.
        /// </summary>
        public float z;
        /// <summary>
        /// W component of the quaternion. Do not modify unless you know what you are doing.
        /// </summary>
        public float w;

        public static readonly Quaternion identity = new Quaternion(0f, 0f, 0f, 1f);

        public Quaternion(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public float this[int index]
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get
            {
                switch (index)
                {
                    case 0: return x;
                    case 1: return y;
                    case 2: return z;
                    case 3: return w;
                    default: throw new IndexOutOfRangeException("Invalid quaternion index");
                }
            }
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            set
            {
                switch (index)
                {
                    case 0: x = value; break;
                    case 1: y = value; break;
                    case 2: z = value; break;
                    case 3: w = value; break;
                    default: throw new IndexOutOfRangeException("Invalid quaternion index");
                }
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Quaternion operator*(Quaternion lhs, Quaternion rhs)
        {
            return new Quaternion(
                lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
                lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
                lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z);
        }

        /// <summary>
        /// Rotates a point with rotation.
        /// </summary>
        /// <param name="rotation">How much to rotate the point</param>
        /// <param name="point">The point to rotate</param>
        /// <returns></returns>
        public static Vector3 operator*(Quaternion rotation, Vector3 point)
        {
            float x = rotation.x * 2F;
            float y = rotation.y * 2F;
            float z = rotation.z * 2F;
            float xx = rotation.x * x;
            float yy = rotation.y * y;
            float zz = rotation.z * z;
            float xy = rotation.x * y;
            float xz = rotation.x * z;
            float yz = rotation.y * z;
            float wx = rotation.w * x;
            float wy = rotation.w * y;
            float wz = rotation.w * z;

            Vector3 res;
            res.x = (1F - (yy + zz)) * point.x + (xy - wz) * point.y + (xz + wy) * point.z;
            res.y = (xy + wz) * point.x + (1F - (xx + zz)) * point.y + (yz - wx) * point.z;
            res.z = (xz - wy) * point.x + (yz + wx) * point.y + (1F - (xx + yy)) * point.z;
            return res;
        }

        public const float kEpsilon = 0.000001F;

        // Is the dot product of two quaternions within tolerance for them to be considered equal?
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool IsEqualUsingDot(float dot)
        {
            // Returns false in the presence of NaN values.
            return dot > 1.0f - kEpsilon;
        }

        // Are two quaternions equal to each other?
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool operator ==(Quaternion lhs, Quaternion rhs)
        {
            return IsEqualUsingDot(Dot(lhs, rhs));
        }

        // Are two quaternions different from each other?
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool operator !=(Quaternion lhs, Quaternion rhs)
        {
            // Returns true in the presence of NaN values.
            return !(lhs == rhs);
        }

        // The dot product between two rotations.
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Dot(Quaternion a, Quaternion b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        }
    }
}