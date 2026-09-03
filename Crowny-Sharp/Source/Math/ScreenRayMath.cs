using System;

namespace Crowny
{
    /// <summary>
    /// Pure math for converting a screen-space point to a world-space ray. Kept free of
    /// native host calls so it can be unit tested in isolation. Matrices use the
    /// engine's column-major layout and match the renderer's conventions (OpenGL clip
    /// space, depth in [-1, 1], y-up).
    /// </summary>
    public static class ScreenRayMath
    {
        /// <summary>
        /// Computes the world-space ray that passes through the given screen point.
        /// The point is expressed in pixels with the origin at the bottom-left corner
        /// of the render target, matching Unity's screen coordinate convention.
        /// </summary>
        /// <param name="projection">The camera's projection matrix, as used for rendering.</param>
        /// <param name="view">The camera's world-to-view matrix, as used for rendering.</param>
        /// <param name="screenWidth">The render target width in pixels.</param>
        /// <param name="screenHeight">The render target height in pixels.</param>
        /// <param name="screenX">The horizontal screen coordinate in pixels.</param>
        /// <param name="screenY">The vertical screen coordinate in pixels, counted from the bottom.</param>
        /// <returns>A ray starting on the camera's near plane pointing through the screen point.</returns>
        public static Ray ComputeRay(Matrix4 projection, Matrix4 view, float screenWidth, float screenHeight,
                                     float screenX, float screenY)
        {
            // Degenerate sizes (collapsed panels, headless) still produce a valid ray down the screen center.
            if (screenWidth <= 0.0f)
                screenWidth = 1.0f;
            if (screenHeight <= 0.0f)
                screenHeight = 1.0f;

            const float ndcNear = -1.0f;
            const float ndcFar = 1.0f;
            float ndcX = 2.0f * screenX / screenWidth - 1.0f;
            float ndcY = 2.0f * screenY / screenHeight - 1.0f;

            Matrix4 inverseProjection = Invert(projection);
            Matrix4 inverseView = Invert(view);

            Vector4 nearView = MultiplyVector4(inverseProjection, new Vector4(ndcX, ndcY, ndcNear, 1.0f));
            Vector4 farView = MultiplyVector4(inverseProjection, new Vector4(ndcX, ndcY, ndcFar, 1.0f));

            Vector4 nearPoint = nearView / nearView.w;
            Vector4 farPoint = farView / farView.w;
            Vector3 worldNear = MultiplyPoint(inverseView, nearPoint);
            Vector3 worldFar = MultiplyPoint(inverseView, farPoint);

            return new Ray(worldNear, worldFar - worldNear);
        }

        /// <summary>Transforms a point by the matrix (column-major, w = 1).</summary>
        private static Vector3 MultiplyPoint(Matrix4 m, Vector4 v)
        {
            return new Vector3(m[0, 0] * v.x + m[0, 1] * v.y + m[0, 2] * v.z + m[0, 3] * v.w,
                               m[1, 0] * v.x + m[1, 1] * v.y + m[1, 2] * v.z + m[1, 3] * v.w,
                               m[2, 0] * v.x + m[2, 1] * v.y + m[2, 2] * v.z + m[2, 3] * v.w);
        }

        /// <summary>Multiplies the matrix by a vector, preserving the resulting w component.</summary>
        private static Vector4 MultiplyVector4(Matrix4 m, Vector4 v)
        {
            return new Vector4(m[0, 0] * v.x + m[0, 1] * v.y + m[0, 2] * v.z + m[0, 3] * v.w,
                               m[1, 0] * v.x + m[1, 1] * v.y + m[1, 2] * v.z + m[1, 3] * v.w,
                               m[2, 0] * v.x + m[2, 1] * v.y + m[2, 2] * v.z + m[2, 3] * v.w,
                               m[3, 0] * v.x + m[3, 1] * v.y + m[3, 2] * v.z + m[3, 3] * v.w);
        }

        /// <summary>Gauss-Jordan inversion with partial pivoting. Returns identity for singular matrices.</summary>
        public static Matrix4 Invert(Matrix4 m)
        {
            float[][] a = new float[4][];
            float[][] inv = new float[4][];
            for (int i = 0; i < 4; i++)
            {
                a[i] = new float[4];
                inv[i] = new float[4];
                for (int j = 0; j < 4; j++)
                {
                    a[i][j] = m[i, j];
                    inv[i][j] = i == j ? 1.0f : 0.0f;
                }
            }

            for (int col = 0; col < 4; col++)
            {
                int pivotRow = col;
                float pivotValue = Math.Abs(a[col][col]);
                for (int row = col + 1; row < 4; row++)
                {
                    float candidate = Math.Abs(a[row][col]);
                    if (candidate > pivotValue)
                    {
                        pivotValue = candidate;
                        pivotRow = row;
                    }
                }

                if (pivotValue < 1e-10f)
                    return Matrix4.identity;

                if (pivotRow != col)
                {
                    float[] tempRow = a[pivotRow];
                    a[pivotRow] = a[col];
                    a[col] = tempRow;
                    float[] tempInv = inv[pivotRow];
                    inv[pivotRow] = inv[col];
                    inv[col] = tempInv;
                }

                float pivot = a[col][col];
                for (int j = 0; j < 4; j++)
                {
                    a[col][j] /= pivot;
                    inv[col][j] /= pivot;
                }

                for (int row = 0; row < 4; row++)
                {
                    if (row == col)
                        continue;
                    float factor = a[row][col];
                    if (factor == 0.0f)
                        continue;
                    for (int j = 0; j < 4; j++)
                    {
                        a[row][j] -= factor * a[col][j];
                        inv[row][j] -= factor * inv[col][j];
                    }
                }
            }

            Matrix4 result = Matrix4.identity;
            for (int row = 0; row < 4; row++)
            {
                for (int col2 = 0; col2 < 4; col2++)
                    result[row, col2] = inv[row][col2];
            }
            return result;
        }
    }
}
