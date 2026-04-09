using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public class Mesh : Asset
    {
        public uint vertexCount { get { return Internal_GetVertexCount(m_InternalPtr); } }
        public uint indexCount { get { return Internal_GetIndexCount(m_InternalPtr); } }

        public Vector3[] vertices
        {
            get { Internal_GetVertices(m_InternalPtr, out Vector3[] result); return result; }
            set { Internal_SetVertices(m_InternalPtr, value); }
        }

        public int[] triangles
        {
            get { Internal_GetIndices(m_InternalPtr, out int[] result); return result; }
            set { Internal_SetIndices(m_InternalPtr, value); }
        }

        public Vector3[] normals
        {
            get { Internal_GetNormals(m_InternalPtr, out Vector3[] result); return result; }
            set { Internal_SetNormals(m_InternalPtr, value); }
        }

        public Vector2[] uv
        {
            get { Internal_GetUVs(m_InternalPtr, 0, out Vector2[] result); return result; }
            set { Internal_SetUVs(m_InternalPtr, 0, value); }
        }

        public Vector2[] uv2
        {
            get { Internal_GetUVs(m_InternalPtr, 1, out Vector2[] result); return result; }
            set { Internal_SetUVs(m_InternalPtr, 1, value); }
        }

        public Vector4[] colors
        {
            get { Internal_GetColors(m_InternalPtr, out Vector4[] result); return result; }
            set { Internal_SetColors(m_InternalPtr, value); }
        }

        public Vector3 boundsMin { get { Internal_GetBoundsMin(m_InternalPtr, out Vector3 min); return min; } }
        public Vector3 boundsMax { get { Internal_GetBoundsMax(m_InternalPtr, out Vector3 max); return max; } }

        public void RecalculateBounds() => Internal_RecalculateBounds(m_InternalPtr);
        public void RecalculateNormals() => Internal_RecalculateNormals(m_InternalPtr);
        public void UploadMeshData() => Internal_UploadMeshData(m_InternalPtr);
        public void Clear() => Internal_Clear(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetVertexCount(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetIndexCount(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetVertices(IntPtr thisPtr, out Vector3[] vertices);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVertices(IntPtr thisPtr, Vector3[] vertices);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetNormals(IntPtr thisPtr, out Vector3[] normals);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetNormals(IntPtr thisPtr, Vector3[] normals);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetUVs(IntPtr thisPtr, uint channel, out Vector2[] uvs);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetUVs(IntPtr thisPtr, uint channel, Vector2[] uvs);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetColors(IntPtr thisPtr, out Vector4[] colors);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetColors(IntPtr thisPtr, Vector4[] colors);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetIndices(IntPtr thisPtr, out int[] indices);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetIndices(IntPtr thisPtr, int[] indices);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_RecalculateBounds(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_RecalculateNormals(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_UploadMeshData(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Clear(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetBoundsMin(IntPtr thisPtr, out Vector3 min);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetBoundsMax(IntPtr thisPtr, out Vector3 max);
    }
}
