using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    static internal class MathBindings
    {
        public static float Matrix4Determinant(Matrix4 matrix)
        {
            return ManagedRuntimeContext.MathMatrixDeterminant(matrix);
        }

        public static Matrix4 Matrix4Inverse(Matrix4 matrix)
        {
            return ManagedRuntimeContext.MathMatrixInverse(matrix);
        }

        public static Matrix4 Matrix4InverseAffine(Matrix4 matrix)
        {
            return ManagedRuntimeContext.MathMatrixAffineInverse(matrix);
        }

        public static Matrix4 LookAt(Vector3 from, Vector3 to, Vector3 up)
        {
            return ManagedRuntimeContext.MathLookAt(from, to, up);
        }

    }

    public partial struct Matrix4
    {
        public float determinant { get { return MathBindings.Matrix4Determinant(this); } }
        public Matrix4 inverse { get { return MathBindings.Matrix4Inverse(this); } }
        public Matrix4 affineInverse { get { return MathBindings.Matrix4InverseAffine(this); } }

        public Matrix4 LookAt(Vector3 from, Vector3 to, Vector3 up) { return MathBindings.LookAt(from, to, up); }
    };
}
