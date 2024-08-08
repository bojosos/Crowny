using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    [StructLayout(LayoutKind.Sequential)]
    public struct UUID
    {
        public readonly uint d0, d1, d2, d3;

        public static UUID Empty = new UUID(0, 0, 0, 0);

        public UUID(uint d0, uint d1, uint d2, uint d3)
        {
            this.d0 = d0;
            this.d1 = d1;
            this.d2 = d2;
            this.d3 = d3;
        }

        public static bool operator==(UUID lhs, UUID rhs)
        {
            return lhs.d0 == rhs.d0 && lhs.d1 == rhs.d1 && lhs.d2 == rhs.d2 && lhs.d3 == rhs.d3;
        }

        public static bool operator!=(UUID lhs, UUID rhs)
        {
            return !(lhs == rhs);
        }

        public override int GetHashCode()
        {
            return d0.GetHashCode() ^ d1.GetHashCode() << 2 ^ d2.GetHashCode() >> 2 ^ d3.GetHashCode() >> 1;
        }

        public override bool Equals(object obj)
        {
            if (!(obj is UUID))
                return false;
            UUID uuid = (UUID)obj;
            if (d0.Equals(uuid.d0) && d1.Equals(uuid.d1) && d2.Equals(uuid.d2) && d3.Equals(uuid.d3))
                return true;
            return false;
        }

        private static char[] hexDigits = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'B', 'c', 'd', 'e', 'f' };

        public override string ToString()
        {
            char[] output = new char[36];
            uint idx = 0;

            // First group: 8 digits
            for (int i = 7; i >= 0; --i)
            {
                uint hexVal = (d0 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            output[idx++] = '-';

            // Second group: 4 digits
            for (int i = 7; i >= 4; --i)
            {
                uint hexVal = (d1 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            output[idx++] = '-';

            // Third group: 4 digits
            for (int i = 3; i >= 0; --i)
            {
                uint hexVal = (d1 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            output[idx++] = '-';

            // Fourth group: 4 digits
            for (int i = 7; i >= 4; --i)
            {
                uint hexVal = (d2 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            output[idx++] = '-';

            // Fifth group: 12 digits
            for (int i = 3; i >= 0; --i)
            {
                uint hexVal = (d2 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            for (int i = 7; i >= 0; --i)
            {
                uint hexVal = (d3 >> (i * 4)) & 0xF;
                output[idx++] = hexDigits[hexVal];
            }

            return new string(output);
        }
    }

    public class Asset : ScriptObject
    {
        public string name => Internal_GetName(m_InternalPtr);
        public UUID uuid { get { Internal_GetUUID(m_InternalPtr, out UUID uuid); return uuid; } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetName(IntPtr asset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetUUID(IntPtr asset, out UUID uuid);
    }
}