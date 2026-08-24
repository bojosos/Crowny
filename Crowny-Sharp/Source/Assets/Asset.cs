using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    public enum PhysicsCombineMode : byte
    {
        GeometricMean = 0,
        Average = 1,
        Minimum = 2,
        Multiply = 3,
        Maximum = 4
    }

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

    /// <summary>Reusable material assigned to a 2D collider.</summary>
    public sealed class PhysicsMaterial2D : Asset
    {
        public float Density
        {
            get { return Internal_GetDensity(m_InternalPtr); }
            set { Internal_SetDensity(m_InternalPtr, value); }
        }

        public float Friction
        {
            get { return Internal_GetFriction(m_InternalPtr); }
            set { Internal_SetFriction(m_InternalPtr, value); }
        }

        public float Restitution
        {
            get { return Internal_GetRestitution(m_InternalPtr); }
            set { Internal_SetRestitution(m_InternalPtr, value); }
        }

        public float RestitutionThreshold
        {
            get { return Internal_GetRestitutionThreshold(m_InternalPtr); }
            set { Internal_SetRestitutionThreshold(m_InternalPtr, value); }
        }

        public PhysicsCombineMode FrictionCombine
        {
            get { return Internal_GetFrictionCombine(m_InternalPtr); }
            set { Internal_SetFrictionCombine(m_InternalPtr, value); }
        }

        public PhysicsCombineMode RestitutionCombine
        {
            get { return Internal_GetRestitutionCombine(m_InternalPtr); }
            set { Internal_SetRestitutionCombine(m_InternalPtr, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetDensity(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetDensity(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetFriction(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFriction(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRestitution(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitution(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRestitutionThreshold(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitutionThreshold(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsCombineMode Internal_GetFrictionCombine(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFrictionCombine(IntPtr material, PhysicsCombineMode value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsCombineMode Internal_GetRestitutionCombine(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitutionCombine(IntPtr material, PhysicsCombineMode value);
    }

    /// <summary>Reusable material assigned to a 3D collider.</summary>
    public sealed class PhysicsMaterial3D : Asset
    {
        public float Density
        {
            get { return Internal_GetDensity(m_InternalPtr); }
            set { Internal_SetDensity(m_InternalPtr, value); }
        }

        public float Friction
        {
            get { return Internal_GetFriction(m_InternalPtr); }
            set { Internal_SetFriction(m_InternalPtr, value); }
        }

        public float Restitution
        {
            get { return Internal_GetRestitution(m_InternalPtr); }
            set { Internal_SetRestitution(m_InternalPtr, value); }
        }

        public float RestitutionThreshold
        {
            get { return Internal_GetRestitutionThreshold(m_InternalPtr); }
            set { Internal_SetRestitutionThreshold(m_InternalPtr, value); }
        }

        public PhysicsCombineMode FrictionCombine
        {
            get { return Internal_GetFrictionCombine(m_InternalPtr); }
            set { Internal_SetFrictionCombine(m_InternalPtr, value); }
        }

        public PhysicsCombineMode RestitutionCombine
        {
            get { return Internal_GetRestitutionCombine(m_InternalPtr); }
            set { Internal_SetRestitutionCombine(m_InternalPtr, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetDensity(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetDensity(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetFriction(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFriction(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRestitution(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitution(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRestitutionThreshold(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitutionThreshold(IntPtr material, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsCombineMode Internal_GetFrictionCombine(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFrictionCombine(IntPtr material, PhysicsCombineMode value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsCombineMode Internal_GetRestitutionCombine(IntPtr material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRestitutionCombine(IntPtr material, PhysicsCombineMode value);
    }
}
