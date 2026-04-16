using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    public enum Allocator
    {
        Temp,
        Persistent
    }

    public unsafe struct NativeArray<T> : IDisposable where T : struct
    {
        private IntPtr m_Buffer;
        private int m_Length;
        private Allocator m_Allocator;
        private static readonly int s_ElementSize = Marshal.SizeOf(typeof(T));

        public NativeArray(int length, Allocator allocator)
        {
            m_Length = length;
            m_Allocator = allocator;
            m_Buffer = Marshal.AllocHGlobal(length * s_ElementSize);

            // Zero-initialize
            byte* ptr = (byte*)m_Buffer;
            for (int i = 0; i < length * s_ElementSize; i++)
                ptr[i] = 0;
        }

        public NativeArray(T[] array, Allocator allocator)
        {
            m_Length = array.Length;
            m_Allocator = allocator;
            m_Buffer = Marshal.AllocHGlobal(m_Length * s_ElementSize);

            GCHandle handle = GCHandle.Alloc(array, GCHandleType.Pinned);
            try
            {
                Buffer.MemoryCopy((void*)handle.AddrOfPinnedObject(), (void*)m_Buffer, m_Length * s_ElementSize, m_Length * s_ElementSize);
            }
            finally
            {
                handle.Free();
            }
        }

        public bool IsCreated => m_Buffer != IntPtr.Zero;

        public int Length => m_Length;

        public T this[int index]
        {
            get
            {
#if CW_DEBUG
                if (m_Buffer == IntPtr.Zero)
                    throw new ObjectDisposedException("NativeArray has been disposed");
                if ((uint)index >= (uint)m_Length)
                    throw new IndexOutOfRangeException($"Index {index} out of range [0, {m_Length})");
#endif
                byte* ptr = (byte*)m_Buffer + index * s_ElementSize;
                return Marshal.PtrToStructure<T>((IntPtr)ptr);
            }
            set
            {
#if CW_DEBUG
                if (m_Buffer == IntPtr.Zero)
                    throw new ObjectDisposedException("NativeArray has been disposed");
                if ((uint)index >= (uint)m_Length)
                    throw new IndexOutOfRangeException($"Index {index} out of range [0, {m_Length})");
#endif
                byte* ptr = (byte*)m_Buffer + index * s_ElementSize;
                Marshal.StructureToPtr(value, (IntPtr)ptr, false);
            }
        }

        /// <summary>
        /// Returns the raw pointer to the underlying buffer. Valid until Dispose is called.
        /// </summary>
        public IntPtr GetUnsafePtr()
        {
#if CW_DEBUG
            if (m_Buffer == IntPtr.Zero)
                throw new ObjectDisposedException("NativeArray has been disposed");
#endif
            return m_Buffer;
        }

        /// <summary>
        /// Copies from a managed array into this NativeArray.
        /// </summary>
        public void CopyFrom(T[] array)
        {
#if CW_DEBUG
            if (m_Buffer == IntPtr.Zero)
                throw new ObjectDisposedException("NativeArray has been disposed");
            if (array.Length != m_Length)
                throw new ArgumentException($"Array length {array.Length} does not match NativeArray length {m_Length}");
#endif
            GCHandle handle = GCHandle.Alloc(array, GCHandleType.Pinned);
            try
            {
                Buffer.MemoryCopy((void*)handle.AddrOfPinnedObject(), (void*)m_Buffer, m_Length * s_ElementSize, m_Length * s_ElementSize);
            }
            finally
            {
                handle.Free();
            }
        }

        /// <summary>
        /// Copies from this NativeArray into a managed array.
        /// </summary>
        public void CopyTo(T[] array)
        {
#if CW_DEBUG
            if (m_Buffer == IntPtr.Zero)
                throw new ObjectDisposedException("NativeArray has been disposed");
            if (array.Length != m_Length)
                throw new ArgumentException($"Array length {array.Length} does not match NativeArray length {m_Length}");
#endif
            GCHandle handle = GCHandle.Alloc(array, GCHandleType.Pinned);
            try
            {
                Buffer.MemoryCopy((void*)m_Buffer, (void*)handle.AddrOfPinnedObject(), m_Length * s_ElementSize, m_Length * s_ElementSize);
            }
            finally
            {
                handle.Free();
            }
        }

        /// <summary>
        /// Returns a managed array copy of this NativeArray.
        /// </summary>
        public T[] ToArray()
        {
            T[] array = new T[m_Length];
            CopyTo(array);
            return array;
        }

        public void Dispose()
        {
            if (m_Buffer != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(m_Buffer);
                m_Buffer = IntPtr.Zero;
            }
            m_Length = 0;
        }
    }
}
