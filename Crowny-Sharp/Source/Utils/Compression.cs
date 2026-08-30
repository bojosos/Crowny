using System;

namespace Crowny
{

    public enum CompressionMethod
    {
        FastLZ = 0,
        Deflate = 1,
        Zstd = 2
    }

    /// <summary>
    /// Selects the FastLZ compression level.
    /// </summary>
    public enum FastLZLevel
    {
        Fastest = 1,
        BetterRatio = 2
    }

    public static class Compression
    {
        /// <summary>
        /// Returns the destination size required for worst-case compression.
        /// </summary>
        public static int MaxCompressedSize(int sourceSize, CompressionMethod method = CompressionMethod.FastLZ)
        {
            ValidateMethod(method);
            if (sourceSize < 0)
                throw new ArgumentOutOfRangeException(nameof(sourceSize));
            if (sourceSize < 16)
                return sourceSize;

            long result = Math.Max(66L, sourceSize + (sourceSize + 19L) / 20L);
            if (result > int.MaxValue)
                throw new ArgumentOutOfRangeException(nameof(sourceSize));
            return (int)result;
        }

        /// <summary>
        /// Compresses into a caller-owned destination and returns the bytes written.
        /// </summary>
        public static ulong Compress(byte[] dst, byte[] src, CompressionMethod method = CompressionMethod.FastLZ,
            FastLZLevel level = FastLZLevel.Fastest)
        {
            ValidateMethod(method);
            ValidateLevel(level);
            if (dst == null)
                throw new ArgumentNullException(nameof(dst));
            if (src == null)
                throw new ArgumentNullException(nameof(src));
            if (dst.Length < MaxCompressedSize(src.Length, method))
                throw new ArgumentException("The destination is too small for the worst-case compressed data.", nameof(dst));

            ulong result = ManagedRuntimeContext.CompressionCompress(dst, src, (int)method, (int)level);
            ThrowIfFailed(result, "Compression failed.");
            return result;
        }

        /// <summary>
        /// Compresses a byte array and returns an exactly sized result.
        /// </summary>
        public static byte[] Compress(byte[] src, CompressionMethod method = CompressionMethod.FastLZ,
            FastLZLevel level = FastLZLevel.Fastest)
        {
            if (src == null)
                throw new ArgumentNullException(nameof(src));

            byte[] result = new byte[MaxCompressedSize(src.Length, method)];
            int resultSize = checked((int)Compress(result, src, method, level));
            Array.Resize(ref result, resultSize);
            return result;
        }

        /// <summary>
        /// Decompresses into a caller-owned destination and returns the bytes written.
        /// </summary>
        public static ulong Decompress(byte[] dst, int maxDstSize, byte[] src, int srcSize,
            CompressionMethod method = CompressionMethod.FastLZ)
        {
            ValidateMethod(method);
            if (dst == null)
                throw new ArgumentNullException(nameof(dst));
            if (src == null)
                throw new ArgumentNullException(nameof(src));
            if (maxDstSize < 0 || maxDstSize > dst.Length)
                throw new ArgumentOutOfRangeException(nameof(maxDstSize));
            if (srcSize < 0 || srcSize > src.Length)
                throw new ArgumentOutOfRangeException(nameof(srcSize));

            ulong result = ManagedRuntimeContext.CompressionDecompress(dst, (ulong)maxDstSize, src, (ulong)srcSize, (int)method);
            ThrowIfFailed(result, "The compressed data is invalid or the destination is too small.");
            return result;
        }

        /// <summary>
        /// Decompresses a byte array to the requested original size.
        /// </summary>
        public static byte[] Decompress(byte[] src, int decompressedSize, CompressionMethod method = CompressionMethod.FastLZ)
        {
            if (src == null)
                throw new ArgumentNullException(nameof(src));
            if (decompressedSize < 0)
                throw new ArgumentOutOfRangeException(nameof(decompressedSize));

            byte[] result = new byte[decompressedSize];
            ulong resultSize = Decompress(result, result.Length, src, src.Length, method);
            if (resultSize != (ulong)decompressedSize)
                throw new InvalidOperationException("The decompressed size does not match the requested size.");
            return result;
        }

        private static void ValidateMethod(CompressionMethod method)
        {
            if (method != CompressionMethod.FastLZ)
                throw new NotSupportedException($"{method} compression is not implemented.");
        }

        private static void ValidateLevel(FastLZLevel level)
        {
            if (level != FastLZLevel.Fastest && level != FastLZLevel.BetterRatio)
                throw new ArgumentOutOfRangeException(nameof(level));
        }

        private static void ThrowIfFailed(ulong result, string message)
        {
            if (result == ulong.MaxValue)
                throw new InvalidOperationException(message);
        }
    }
}
