using System.Runtime.CompilerServices;

namespace Crowny
{

    public enum CompressionMethod
    {
        FastLZ = 0,
        Deflate = 1,
        Zstd = 2
    }

    public static class Compression
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static ulong Internal_Compress(byte[] dst, byte[] src, CompressionMethod method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern static ulong Internal_Decompress(byte[] dst, int maxDstSize, byte[] src, int srcSize, CompressionMethod method);

        public static ulong Compress(byte[] dst, byte[] src, CompressionMethod method = CompressionMethod.Zstd)
        {
            return Internal_Compress(dst, src, method);
        }

        public static ulong Decompress(byte[] dst, int maxDstSize, byte[] src, int srcSize, CompressionMethod method = CompressionMethod.Zstd)
        {
            return Internal_Decompress(dst, maxDstSize, src, srcSize, method);
        }
    }
}
