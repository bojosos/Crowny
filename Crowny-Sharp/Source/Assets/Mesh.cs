using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>CPU-accessible mesh data shared by every managed runtime backend.</summary>
    public class Mesh : Asset
    {
        public uint vertexCount { get { return ManagedRuntimeContext.MeshGetVertexCount(m_ManagedUuid); } }
        public uint indexCount { get { return ManagedRuntimeContext.MeshGetIndexCount(m_ManagedUuid); } }

        public Vector3[] vertices
        {
            get { return ManagedArrayInterop.Read<Vector3>(vertexCount, (data, count) => ManagedRuntimeContext.MeshCopyVertices(m_ManagedUuid, data, count)); }
            set { ManagedArrayInterop.WithPinned(value, (data, count) => ManagedRuntimeContext.MeshSetVertices(m_ManagedUuid, data, count)); }
        }

        public int[] triangles
        {
            get { return ManagedArrayInterop.Read<int>(indexCount, (data, count) => ManagedRuntimeContext.MeshCopyIndices(m_ManagedUuid, data, count)); }
            set { ManagedArrayInterop.WithPinned(value, (data, count) => ManagedRuntimeContext.MeshSetIndices(m_ManagedUuid, data, count)); }
        }

        public Vector3[] normals
        {
            get { return ManagedArrayInterop.Read<Vector3>(vertexCount, (data, count) => ManagedRuntimeContext.MeshCopyNormals(m_ManagedUuid, data, count)); }
            set { ManagedArrayInterop.WithPinned(value, (data, count) => ManagedRuntimeContext.MeshSetNormals(m_ManagedUuid, data, count)); }
        }

        public Vector2[] uv
        {
            get { return ReadUvs(0); }
            set { WriteUvs(0, value); }
        }

        public Vector2[] uv2
        {
            get { return ReadUvs(1); }
            set { WriteUvs(1, value); }
        }

        public Vector4[] colors
        {
            get { return ManagedArrayInterop.Read<Vector4>(vertexCount, (data, count) => ManagedRuntimeContext.MeshCopyColors(m_ManagedUuid, data, count)); }
            set { ManagedArrayInterop.WithPinned(value, (data, count) => ManagedRuntimeContext.MeshSetColors(m_ManagedUuid, data, count)); }
        }

        public Vector3 boundsMin { get { return ManagedRuntimeContext.MeshGetBoundsMin(m_ManagedUuid); } }
        public Vector3 boundsMax { get { return ManagedRuntimeContext.MeshGetBoundsMax(m_ManagedUuid); } }

        public void RecalculateBounds() { ManagedRuntimeContext.MeshRecalculateBounds(m_ManagedUuid); }
        public void RecalculateNormals() { ManagedRuntimeContext.MeshRecalculateNormals(m_ManagedUuid); }
        public void RecalculateTangents() { ManagedRuntimeContext.MeshRecalculateTangents(m_ManagedUuid); }
        public void UploadMeshData() { ManagedRuntimeContext.MeshUploadData(m_ManagedUuid); }
        public void Clear() { ManagedRuntimeContext.MeshClear(m_ManagedUuid); }

        /// <summary>Sets the vertex buffer layout and vertex count.</summary>
        public void SetVertexBufferParams(uint vertexCount, params VertexAttributeDescriptor[] layout)
        {
            if (layout == null)
                throw new ArgumentNullException("layout");
            ManagedArrayInterop.WithPinned(layout, (data, count) =>
                ManagedRuntimeContext.MeshSetVertexBufferParams(m_ManagedUuid, vertexCount, data, count));
        }

        /// <summary>Writes raw interleaved vertex data from a managed array.</summary>
        public void SetVertexBufferData<T>(T[] data, int dataStart, int meshBufferStart, int count) where T : struct
        {
            ValidateRange(data, dataStart, count);
            if (meshBufferStart < 0)
                throw new ArgumentOutOfRangeException("meshBufferStart");
            int stride = Marshal.SizeOf(typeof(T));
            ManagedArrayInterop.WithPinned(data, (pointer, ignored) =>
            {
                IntPtr source = count == 0 ? IntPtr.Zero : IntPtr.Add(pointer, checked(dataStart * stride));
                ManagedRuntimeContext.MeshSetVertexBufferData(m_ManagedUuid, source, checked((uint)meshBufferStart),
                                                              checked((uint)count), checked((uint)stride));
            });
        }

        /// <summary>Writes raw interleaved vertex data from a native array.</summary>
        public void SetVertexBufferData<T>(NativeArray<T> data, int dataStart, int meshBufferStart, int count) where T : struct
        {
            if (dataStart < 0 || count < 0 || dataStart > data.Length - count)
                throw new ArgumentOutOfRangeException("dataStart");
            if (meshBufferStart < 0)
                throw new ArgumentOutOfRangeException("meshBufferStart");
            int stride = Marshal.SizeOf(typeof(T));
            IntPtr source = count == 0 ? IntPtr.Zero : IntPtr.Add(data.GetUnsafePtr(), checked(dataStart * stride));
            ManagedRuntimeContext.MeshSetVertexBufferData(m_ManagedUuid, source, checked((uint)meshBufferStart),
                                                          checked((uint)count), checked((uint)stride));
        }

        /// <summary>Reads raw interleaved vertex data into a new managed array.</summary>
        public T[] GetVertexBufferData<T>() where T : struct
        {
            return ManagedArrayInterop.Read<T>(vertexCount, (data, count) =>
                ManagedRuntimeContext.MeshGetVertexBufferData(m_ManagedUuid, data, count,
                                                              checked((uint)Marshal.SizeOf(typeof(T)))));
        }

        /// <summary>Reads raw interleaved vertex data into an existing native array.</summary>
        public void GetVertexBufferData<T>(NativeArray<T> dest) where T : struct
        {
            ManagedRuntimeContext.MeshGetVertexBufferData(m_ManagedUuid, dest.GetUnsafePtr(),
                                                          checked((uint)dest.Length),
                                                          checked((uint)Marshal.SizeOf(typeof(T))));
        }

        public uint vertexStride { get { return ManagedRuntimeContext.MeshGetVertexStride(m_ManagedUuid); } }
        public uint vertexAttributeCount { get { return ManagedRuntimeContext.MeshGetVertexAttributeCount(m_ManagedUuid); } }

        public bool HasVertexAttribute(VertexAttribute attr)
        {
            return ManagedRuntimeContext.MeshHasVertexAttribute(m_ManagedUuid, (int)attr);
        }

        public VertexAttributeDescriptor GetVertexAttribute(int index)
        {
            if (index < 0 || (uint)index >= vertexAttributeCount)
                throw new ArgumentOutOfRangeException("index");
            VertexAttributeDescriptor[] descriptor = new VertexAttributeDescriptor[1];
            ManagedArrayInterop.WithPinned(descriptor, (data, ignored) => ManagedRuntimeContext.MeshGetVertexAttribute(m_ManagedUuid, index, data));
            return descriptor[0];
        }

        /// <summary>Creates an XZ plane centered at the origin.</summary>
        public static Mesh CreatePlane(float width = 1.0f, float height = 1.0f, uint subdivisionsX = 1, uint subdivisionsY = 1)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(
                ManagedRuntimeContext.MeshCreatePlane(width, height, subdivisionsX, subdivisionsY), true);
        }

        /// <summary>Creates a single XZ quad centered at the origin.</summary>
        public static Mesh CreateQuad(float width = 1.0f, float height = 1.0f)
        {
            return CreatePlane(width, height, 1, 1);
        }

        /// <summary>Creates a box centered at the origin.</summary>
        public static Mesh CreateBox(Vector3 dimensions)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshCreateBox(dimensions), true);
        }

        /// <summary>Creates a cube centered at the origin.</summary>
        public static Mesh CreateCube(float size = 1.0f)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshCreateCube(size), true);
        }

        /// <summary>Creates a UV sphere centered at the origin.</summary>
        public static Mesh CreateSphere(float radius = 0.5f, uint segments = 32, uint rings = 16)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshCreateSphere(radius, segments, rings), true);
        }

        /// <summary>Creates a Y-axis cylinder centered at the origin.</summary>
        public static Mesh CreateCylinder(float radius = 0.5f, float height = 1.0f, uint segments = 32, bool capped = true)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshCreateCylinder(radius, height, segments, capped), true);
        }

        /// <summary>Creates a Y-axis cone centered at the origin.</summary>
        public static Mesh CreateCone(float radius = 0.5f, float height = 1.0f, uint segments = 32, bool capped = true)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshCreateCone(radius, height, segments, capped), true);
        }

        /// <summary>Creates a Y-axis capsule. Height includes both hemispheres.</summary>
        public static Mesh CreateCapsule(float radius = 0.5f, float height = 2.0f, uint segments = 32, uint hemisphereRings = 8)
        {
            return ManagedRuntimeContext.CreateAsset<Mesh>(
                ManagedRuntimeContext.MeshCreateCapsule(radius, height, segments, hemisphereRings), true);
        }

        private Vector2[] ReadUvs(uint channel)
        {
            return ManagedArrayInterop.Read<Vector2>(vertexCount, (data, count) =>
                ManagedRuntimeContext.MeshCopyUvs(m_ManagedUuid, channel, data, count));
        }

        private void WriteUvs(uint channel, Vector2[] value)
        {
            ManagedArrayInterop.WithPinned(value, (data, count) => ManagedRuntimeContext.MeshSetUvs(m_ManagedUuid, channel, data, count));
        }

        private static void ValidateRange<T>(T[] data, int dataStart, int count)
        {
            if (data == null)
                throw new ArgumentNullException("data");
            if (dataStart < 0 || count < 0 || dataStart > data.Length - count)
                throw new ArgumentOutOfRangeException("dataStart");
        }
    }
}
