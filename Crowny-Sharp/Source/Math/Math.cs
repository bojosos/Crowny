using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    static internal class MathBindings
    {
        public static float Matrix4Determinant(Matrix4 matrix)
        {
            return Internal_Determinant(ref matrix);
        }

        public static Matrix4 Matrix4Inverse(Matrix4 matrix)
        {
            Internal_Inverse(ref matrix, out  Matrix4 outMatrix);
            return outMatrix;
        }

        public static Matrix4 Matrix4InverseAffine(Matrix4 matrix)
        {
            Internal_InverseAffine(ref matrix, out Matrix4 outMatrix);
            return outMatrix;
        }

        public static Matrix4 LookAt(Vector3 from, Vector3 to, Vector3 up)
        {
            Internal_LookAt(from, to, up, out Matrix4 outMatrix);
            return outMatrix;
        }


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_Determinant(ref Matrix4 matrix);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Matrix4 Internal_Inverse(ref Matrix4 matrix, out Matrix4 outMatrix);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Matrix4 Internal_InverseAffine(ref Matrix4 matrix, out Matrix4 outMatrix);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Matrix4 Internal_LookAt(Vector3 from, Vector3 to, Vector3 up, out Matrix4 outMatirx);
    }

    public partial struct Matrix4
    {
        public float determinant {  get { return MathBindings.Matrix4Determinant(this); } }
        public Matrix4 inverse {  get { return MathBindings.Matrix4Inverse(this); } }
        public Matrix4 affineInverse {  get { return MathBindings.Matrix4InverseAffine(this); } }

        public Matrix4 LookAt(Vector3 from, Vector3 to, Vector3 up) { return MathBindings.LookAt(from, to, up); }
    };
}
