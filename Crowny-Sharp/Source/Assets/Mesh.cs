using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

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
        public void RecalculateTangents() => Internal_RecalculateTangents(m_InternalPtr);
        public void UploadMeshData() => Internal_UploadMeshData(m_InternalPtr);
        public void Clear() => Internal_Clear(m_InternalPtr);

        /// <summary>
        /// Sets the vertex buffer layout and vertex count. Call this before SetVertexBufferData.
        /// </summary>
        public void SetVertexBufferParams(uint vertexCount, params VertexAttributeDescriptor[] layout)
        {
            Internal_SetVertexBufferParams(m_InternalPtr, vertexCount, layout);
        }

        /// <summary>
        /// Writes raw interleaved vertex data from a managed array.
        /// </summary>
        public void SetVertexBufferData<T>(T[] data, int dataStart, int meshBufferStart, int count) where T : struct
        {
            GCHandle handle = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                int stride = Marshal.SizeOf(typeof(T));
                IntPtr ptr = handle.AddrOfPinnedObject();
                Internal_SetVertexBufferData(m_InternalPtr, ptr + dataStart * stride, (uint)meshBufferStart, (uint)count, (uint)stride);
            }
            finally
            {
                handle.Free();
            }
        }

        /// <summary>
        /// Writes raw interleaved vertex data from a NativeArray. No GC allocation or pinning needed.
        /// </summary>
        public void SetVertexBufferData<T>(NativeArray<T> data, int dataStart, int meshBufferStart, int count) where T : struct
        {
            int stride = Marshal.SizeOf(typeof(T));
            Internal_SetVertexBufferData(m_InternalPtr, data.GetUnsafePtr() + dataStart * stride, (uint)meshBufferStart, (uint)count, (uint)stride);
        }

        /// <summary>
        /// Reads raw interleaved vertex data into a new managed array.
        /// </summary>
        public T[] GetVertexBufferData<T>() where T : struct
        {
            int stride = Marshal.SizeOf(typeof(T));
            uint count = vertexCount;
            T[] data = new T[count];
            GCHandle handle = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                Internal_GetVertexBufferData(m_InternalPtr, handle.AddrOfPinnedObject(), count, (uint)stride);
            }
            finally
            {
                handle.Free();
            }
            return data;
        }

        /// <summary>
        /// Reads raw interleaved vertex data into an existing NativeArray. No GC allocation or pinning needed.
        /// </summary>
        public void GetVertexBufferData<T>(NativeArray<T> dest) where T : struct
        {
            int stride = Marshal.SizeOf(typeof(T));
            Internal_GetVertexBufferData(m_InternalPtr, dest.GetUnsafePtr(), (uint)dest.Length, (uint)stride);
        }

        /// <summary>
        /// Returns the stride (bytes per vertex) of the current vertex buffer layout.
        /// </summary>
        public uint vertexStride { get { return Internal_GetVertexStride(m_InternalPtr); } }

        /// <summary>
        /// Returns the number of vertex attributes in the current layout.
        /// </summary>
        public uint vertexAttributeCount { get { return Internal_GetVertexAttributeCount(m_InternalPtr); } }

        /// <summary>
        /// Returns whether the mesh has a specific vertex attribute.
        /// </summary>
        public bool HasVertexAttribute(VertexAttribute attr)
        {
            return Internal_HasVertexAttribute(m_InternalPtr, attr);
        }

        /// <summary>
        /// Returns the descriptor for a vertex attribute at the given index.
        /// </summary>
        public VertexAttributeDescriptor GetVertexAttribute(int index)
        {
            Internal_GetVertexAttribute(m_InternalPtr, index, out VertexAttributeDescriptor desc);
            return desc;
        }

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
        private static extern void Internal_RecalculateTangents(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_UploadMeshData(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Clear(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetBoundsMin(IntPtr thisPtr, out Vector3 min);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetBoundsMax(IntPtr thisPtr, out Vector3 max);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVertexBufferParams(IntPtr thisPtr, uint vertexCount, VertexAttributeDescriptor[] layout);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVertexBufferData(IntPtr thisPtr, IntPtr data, uint meshBufferStart, uint count, uint stride);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetVertexBufferData(IntPtr thisPtr, IntPtr outData, uint count, uint stride);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetVertexStride(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetVertexAttributeCount(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasVertexAttribute(IntPtr thisPtr, VertexAttribute attr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetVertexAttribute(IntPtr thisPtr, int index, out VertexAttributeDescriptor desc);
    }
}
