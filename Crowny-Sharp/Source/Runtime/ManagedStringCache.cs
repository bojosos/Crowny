using System;
using System.Text;

namespace Crowny
{
    /// <summary>
    /// Reuses UTF-8 encodings and decoded managed strings at the native host seam.
    /// The cache has a fixed entry count and does not retain large strings.
    /// </summary>
    internal static unsafe class ManagedStringCache
    {
        private const int Ways = 4;
        private const int SetCount = 64;
        private const int Capacity = Ways * SetCount;
        private const int MaximumCachedUtf8Bytes = 4096;

        private static readonly object encodeLock = new object();
        private static readonly object decodeLock = new object();
        private static readonly EncodeEntry[] encodeEntries = new EncodeEntry[Capacity];
        private static readonly DecodeEntry[] decodeEntries = new DecodeEntry[Capacity];
        private static long encodeStamp;
        private static long decodeStamp;

        internal static byte[] Encode(string value)
        {
            string text = value ?? string.Empty;
            if (text.Length == 0)
                return Array.Empty<byte>();

            uint hash = unchecked((uint)StringComparer.Ordinal.GetHashCode(text));
            lock (encodeLock)
            {
                EncodeEntry cached = FindEncoded(text, hash);
                if (cached != null)
                    return cached.Bytes;
            }

            int byteCount = Encoding.UTF8.GetByteCount(text);
            if (byteCount > MaximumCachedUtf8Bytes)
                return Encoding.UTF8.GetBytes(text);

            byte[] bytes = Encoding.UTF8.GetBytes(text);
            lock (encodeLock)
            {
                EncodeEntry cached = FindEncoded(text, hash);
                if (cached != null)
                    return cached.Bytes;

                encodeEntries[SelectEncodeVictim(hash)] = new EncodeEntry(text, bytes, hash, ++encodeStamp);
                return bytes;
            }
        }

        internal static string Decode(byte* data, uint length)
        {
            if (data == null || length == 0)
                return string.Empty;

            int byteCount = checked((int)length);
            if (byteCount > MaximumCachedUtf8Bytes)
                return Encoding.UTF8.GetString(data, byteCount);

            uint hash = Hash(data, byteCount);
            lock (decodeLock)
            {
                DecodeEntry cached = FindDecoded(data, byteCount, hash);
                if (cached != null)
                    return cached.Text;
            }

            string text = Encoding.UTF8.GetString(data, byteCount);
            byte[] bytes = new byte[byteCount];
            for (int index = 0; index < byteCount; ++index)
                bytes[index] = data[index];

            lock (decodeLock)
            {
                DecodeEntry cached = FindDecoded(data, byteCount, hash);
                if (cached != null)
                    return cached.Text;

                decodeEntries[SelectDecodeVictim(hash)] = new DecodeEntry(text, bytes, hash, ++decodeStamp);
                return text;
            }
        }

        private static EncodeEntry FindEncoded(string text, uint hash)
        {
            int first = FirstSlot(hash);
            for (int offset = 0; offset < Ways; ++offset)
            {
                EncodeEntry entry = encodeEntries[first + offset];
                if (entry != null && entry.Hash == hash && string.Equals(entry.Text, text, StringComparison.Ordinal))
                {
                    entry.Stamp = ++encodeStamp;
                    return entry;
                }
            }
            return null;
        }

        private static DecodeEntry FindDecoded(byte* data, int length, uint hash)
        {
            int first = FirstSlot(hash);
            for (int offset = 0; offset < Ways; ++offset)
            {
                DecodeEntry entry = decodeEntries[first + offset];
                if (entry == null || entry.Hash != hash || entry.Bytes.Length != length)
                    continue;

                bool matches = true;
                for (int index = 0; index < length; ++index)
                {
                    if (entry.Bytes[index] == data[index])
                        continue;
                    matches = false;
                    break;
                }
                if (matches)
                {
                    entry.Stamp = ++decodeStamp;
                    return entry;
                }
            }
            return null;
        }

        private static int SelectEncodeVictim(uint hash)
        {
            int first = FirstSlot(hash);
            int victim = first;
            long oldest = long.MaxValue;
            for (int offset = 0; offset < Ways; ++offset)
            {
                int index = first + offset;
                EncodeEntry entry = encodeEntries[index];
                if (entry == null)
                    return index;
                if (entry.Stamp < oldest)
                {
                    oldest = entry.Stamp;
                    victim = index;
                }
            }
            return victim;
        }

        private static int SelectDecodeVictim(uint hash)
        {
            int first = FirstSlot(hash);
            int victim = first;
            long oldest = long.MaxValue;
            for (int offset = 0; offset < Ways; ++offset)
            {
                int index = first + offset;
                DecodeEntry entry = decodeEntries[index];
                if (entry == null)
                    return index;
                if (entry.Stamp < oldest)
                {
                    oldest = entry.Stamp;
                    victim = index;
                }
            }
            return victim;
        }

        private static int FirstSlot(uint hash)
        {
            return (int)(hash & (SetCount - 1)) * Ways;
        }

        private static uint Hash(byte* data, int length)
        {
            uint hash = 2166136261;
            for (int index = 0; index < length; ++index)
                hash = unchecked((hash ^ data[index]) * 16777619);
            return hash;
        }

        private sealed class EncodeEntry
        {
            internal EncodeEntry(string text, byte[] bytes, uint hash, long stamp)
            {
                Text = text;
                Bytes = bytes;
                Hash = hash;
                Stamp = stamp;
            }

            internal readonly string Text;
            internal readonly byte[] Bytes;
            internal readonly uint Hash;
            internal long Stamp;
        }

        private sealed class DecodeEntry
        {
            internal DecodeEntry(string text, byte[] bytes, uint hash, long stamp)
            {
                Text = text;
                Bytes = bytes;
                Hash = hash;
                Stamp = stamp;
            }

            internal readonly string Text;
            internal readonly byte[] Bytes;
            internal readonly uint Hash;
            internal long Stamp;
        }
    }
}
