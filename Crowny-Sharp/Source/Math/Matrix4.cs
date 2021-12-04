using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Globalization;

namespace Crowny
{

    [StructLayout(LayoutKind.Sequential)]
    public partial struct Matrix4 : IFormattable, IEquatable<Matrix4>
    {
        public static readonly Matrix4 zero = new Matrix4();
        public static readonly Matrix4 identity = new Matrix4(new Vector4(1, 0, 0, 0),
                                                              new Vector4(0, 1, 0, 0),
                                                              new Vector4(0, 0, 1, 0),
                                                              new Vector4(0, 0, 0, 1));
        
        //    m00 m10 m20 m30
        //    m01 m11 m21 m31
        //    m02 m12 m22 m32
        //    m03 m13 m23 m33

        public float m00;
        public float m10;
        public float m20;
        public float m30;
        public float m01;
        public float m11;
        public float m21;
        public float m31;
        public float m02;
        public float m12;
        public float m22;
        public float m32;
        public float m03;
        public float m13;
        public float m23;
        public float m33;

        public Matrix4(Vector4 col0, Vector4 col1, Vector4 col2, Vector4 col3)
        {
            m00 = col0.x; m01 = col1.x; m02 = col2.x; m03 = col3.x;
            m10 = col0.y; m11 = col1.y; m12 = col2.y; m13 = col3.y;
            m20 = col0.z; m21 = col1.z; m22 = col2.z; m23 = col3.z;
            m30 = col0.w; m31 = col1.w; m32 = col2.w; m33 = col3.w;
        }

        public float this[int row, int column]
        { 
            get
            {
                return this[row + column * 4];
            }
            set
            {
                this[row + column * 4] = value;
            }
        }

        public float this[int index]
        {
            get
            {
                switch (index)
                {
                    case 0: return m00;
                    case 1: return m10;
                    case 2: return m20;
                    case 3: return m30;
                    case 4: return m01;
                    case 5: return m11;
                    case 6: return m21;
                    case 7: return m31;
                    case 8: return m02;
                    case 9: return m12;
                    case 10: return m22;
                    case 11: return m32;
                    case 12: return m03;
                    case 13: return m13;
                    case 14: return m23;
                    case 15: return m33;
                    default:
                        throw new IndexOutOfRangeException("Invalid ndex!");
                }
            }

            set
            {
                switch (index)
                {
                    case 0: m00 = value; break;
                    case 1: m10 = value; break;
                    case 2: m20 = value; break;
                    case 3: m30 = value; break;
                    case 4: m01 = value; break;
                    case 5: m11 = value; break;
                    case 6: m21 = value; break;
                    case 7: m31 = value; break;
                    case 8: m02 = value; break;
                    case 9: m12 = value; break;
                    case 10: m22 = value; break;
                    case 11: m32 = value; break;
                    case 12: m03 = value; break;
                    case 13: m13 = value; break;
                    case 14: m23 = value; break;
                    case 15: m33 = value; break;
                    default: throw new IndexOutOfRangeException("Invalid index!");
                }
            }
        }

        public Vector4 GetColumn(int index)
        {
            switch(index)
            {
                case 0: return new Vector4(m00, m10, m20, m30);
                case 1: return new Vector4(m01, m11, m21, m31);
                case 2: return new Vector4(m02, m12, m22, m32);
                case 3: return new Vector4(m03, m13, m23, m33);
                default: throw new IndexOutOfRangeException("Invalid index");
            }
        }

        public Vector4 GetRow(int index)
        {
            switch (index)
            {
                case 0: return new Vector4(m00, m01, m02, m03);
                case 1: return new Vector4(m10, m11, m12, m13);
                case 2: return new Vector4(m20, m21, m22, m23);
                case 3: return new Vector4(m30, m31, m32, m33);
                default: throw new IndexOutOfRangeException("Invalid index");
            }
        }

        public void SetColumn(int index, Vector4 column)
        {
            switch (index)
            {
                case 0: m00 = column.x; m10 = column.y; m20 = column.z; m30 = column.w; break;
                case 1: m01 = column.x; m11 = column.y; m21 = column.z; m31 = column.w; break;
                case 2: m02 = column.x; m12 = column.y; m22 = column.z; m32 = column.w; break;
                case 3: m03 = column.x; m13 = column.y; m23 = column.z; m33 = column.w; break;
                default: throw new IndexOutOfRangeException("Invalid index");
            }
        }

        public void SetRow(int index, Vector4 row)
        {
            switch (index)
            {
                case 0: m00 = row.x; m01 = row.y; m02 = row.z; m03 = row.w; break;
                case 1: m10 = row.x; m11 = row.y; m12 = row.z; m13 = row.w; break;
                case 2: m20 = row.x; m21 = row.y; m22 = row.z; m23 = row.w; break;
                case 3: m30 = row.x; m31 = row.y; m32 = row.z; m33 = row.w; break;
                default: throw new IndexOutOfRangeException("Invalid index");
            }
        }

        public static Matrix4 Transpose(Matrix4 matrix)
        {
            Matrix4 result = new Matrix4();
            result.SetRow(0, matrix.GetColumn(0));
            result.SetRow(1, matrix.GetColumn(1));
            result.SetRow(2, matrix.GetColumn(2));
            result.SetRow(3, matrix.GetColumn(3));
            return result;
        }

        public Matrix4 tranpose { get { return Matrix4.Transpose(this); } }

        public static Matrix4 Translate(Vector3 vector)
        {
            Matrix4 result;
            result.m00 = 1f; result.m01 = 0f; result.m02 = 0f; result.m03 = vector.x;
            result.m10 = 0f; result.m11 = 1f; result.m12 = 0f; result.m13 = vector.y;
            result.m20 = 0f; result.m21 = 0f; result.m22 = 1f; result.m23 = vector.z;
            result.m30 = 0f; result.m31 = 0f; result.m32 = 0f; result.m33 = 1f;
            return result;
        }

        // public static Matrix4 Rotate(Quaternion q)

        public static Matrix4 Scale(Vector3 vector)
        {
            Matrix4 result;
            result.m00 = vector.x; result.m01 = 0f; result.m02 = 0f; result.m03 = 0f;
            result.m10 = 0f; result.m11 = vector.y; result.m12 = 0f; result.m13 = 0f;
            result.m20 = 0f; result.m21 = 0f; result.m22 = vector.z; result.m23 = 0f;
            result.m30 = 0f; result.m31 = 0f; result.m32 = 0f; result.m33 = 1f;
            return result;
        }

        public static Matrix4 operator*(Matrix4 lhs, Matrix4 rhs)
        {
            Matrix4 result;
            result.m00 = lhs.m00 * rhs.m00 + lhs.m01 * rhs.m10 + lhs.m02 * rhs.m20 + lhs.m03 * rhs.m30;
            result.m01 = lhs.m00 * rhs.m01 + lhs.m01 * rhs.m11 + lhs.m02 * rhs.m21 + lhs.m03 * rhs.m31;
            result.m02 = lhs.m00 * rhs.m02 + lhs.m01 * rhs.m12 + lhs.m02 * rhs.m22 + lhs.m03 * rhs.m32;
            result.m03 = lhs.m00 * rhs.m03 + lhs.m01 * rhs.m13 + lhs.m02 * rhs.m23 + lhs.m03 * rhs.m33;

            result.m10 = lhs.m10 * rhs.m00 + lhs.m11 * rhs.m10 + lhs.m12 * rhs.m20 + lhs.m13 * rhs.m30;
            result.m11 = lhs.m10 * rhs.m01 + lhs.m11 * rhs.m11 + lhs.m12 * rhs.m21 + lhs.m13 * rhs.m31;
            result.m12 = lhs.m10 * rhs.m02 + lhs.m11 * rhs.m12 + lhs.m12 * rhs.m22 + lhs.m13 * rhs.m32;
            result.m13 = lhs.m10 * rhs.m03 + lhs.m11 * rhs.m13 + lhs.m12 * rhs.m23 + lhs.m13 * rhs.m33;

            result.m20 = lhs.m20 * rhs.m00 + lhs.m21 * rhs.m10 + lhs.m22 * rhs.m20 + lhs.m23 * rhs.m30;
            result.m21 = lhs.m20 * rhs.m01 + lhs.m21 * rhs.m11 + lhs.m22 * rhs.m21 + lhs.m23 * rhs.m31;
            result.m22 = lhs.m20 * rhs.m02 + lhs.m21 * rhs.m12 + lhs.m22 * rhs.m22 + lhs.m23 * rhs.m32;
            result.m23 = lhs.m20 * rhs.m03 + lhs.m21 * rhs.m13 + lhs.m22 * rhs.m23 + lhs.m23 * rhs.m33;

            result.m30 = lhs.m30 * rhs.m00 + lhs.m31 * rhs.m10 + lhs.m32 * rhs.m20 + lhs.m33 * rhs.m30;
            result.m31 = lhs.m30 * rhs.m01 + lhs.m31 * rhs.m11 + lhs.m32 * rhs.m21 + lhs.m33 * rhs.m31;
            result.m32 = lhs.m30 * rhs.m02 + lhs.m31 * rhs.m12 + lhs.m32 * rhs.m22 + lhs.m33 * rhs.m32;
            result.m33 = lhs.m30 * rhs.m03 + lhs.m31 * rhs.m13 + lhs.m32 * rhs.m23 + lhs.m33 * rhs.m33;

            return result;
        }

        public static Vector4 operator*(Matrix4 lhs, Vector4 rhs)
        {
            Vector4 result;
            result.x = lhs.m00 * rhs.x + lhs.m01 * rhs.y + lhs.m02 * rhs.z + lhs.m03 * rhs.w;
            result.y = lhs.m10 * rhs.x + lhs.m11 * rhs.y + lhs.m12 * rhs.z + lhs.m13 * rhs.w;
            result.z = lhs.m20 * rhs.x + lhs.m21 * rhs.y + lhs.m22 * rhs.z + lhs.m23 * rhs.w;
            result.w = lhs.m30 * rhs.x + lhs.m31 * rhs.y + lhs.m32 * rhs.z + lhs.m33 * rhs.w;
            return result;
        }

        public static bool operator==(Matrix4 lhs, Matrix4 rhs)
        {
#pragma warning disable RECS0018 // Comparison of floating point numbers with equality operator
            return lhs.m00 == rhs.m00 && lhs.m01 == rhs.m01 && lhs.m02 == rhs.m02 && lhs.m03 == rhs.m03 &&
                   lhs.m10 == rhs.m10 && lhs.m11 == rhs.m11 && lhs.m12 == rhs.m12 && lhs.m13 == rhs.m13 &&
                   lhs.m20 == rhs.m20 && lhs.m21 == rhs.m21 && lhs.m22 == rhs.m22 && lhs.m23 == rhs.m23 &&
                   lhs.m30 == rhs.m30 && lhs.m31 == rhs.m31 && lhs.m32 == rhs.m32 && lhs.m33 == rhs.m33;
#pragma warning restore RECS0018 // Comparison of floating point numbers with equality operator
        }

        public static bool operator!=(Matrix4 lhs, Matrix4 rhs)
        {
            return !(lhs == rhs);
        }

        public override bool Equals(object obj)
        {
            if (!(obj is Matrix4)) return false;
            return Equals((Matrix4)obj);
        }

        public bool Equals(Matrix4 other)
        {
            return this == other;
        }

        public Vector3 GetPosition()
        {
            return new Vector3(m00, m13, m23);
        }

        public override int GetHashCode()
        {
            return GetColumn(0).GetHashCode() ^ (GetColumn(1).GetHashCode() << 2) ^ (GetColumn(2).GetHashCode() >> 2) ^ (GetColumn(3).GetHashCode() >> 1);
        }

        public override string ToString()
        {
            return ToString(null, null);
        }

        public string ToString(string format)
        {
            return ToString(format, null);
        }

        public string ToString(string format, IFormatProvider formatProvider)
        {
            if (string.IsNullOrEmpty(format))
                format = "F5";
            if (formatProvider == null)
                formatProvider = CultureInfo.InvariantCulture.NumberFormat;
            return String.Format("({0}, {1}, {2}, {3},\n{4}, {5}, {6}, {7}\n{8}, {9}, {10}, {11}\n{12}, {13}, {14}, {15})",
                                   m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33);
        }
    }

}
