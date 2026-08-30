using System;
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
        public string name => ManagedRuntimeContext.AssetGetName(m_ManagedUuid);
        public UUID uuid => m_ManagedUuid;

        internal UUID m_ManagedUuid;
        internal bool m_OwnsManagedLease;

        ~Asset()
        {
            if (m_OwnsManagedLease)
                ManagedRuntimeContext.ReleaseAsset(m_ManagedUuid);
        }

    }

    /// <summary>Reusable material assigned to a 2D collider.</summary>
    public sealed class PhysicsMaterial2D : Asset
    {
        /// <summary>Creates a runtime material that can be assigned to colliders and serialized with a scene.</summary>
        public static PhysicsMaterial2D Create()
        {
            return ManagedRuntimeContext.CreateAsset<PhysicsMaterial2D>(ManagedRuntimeContext.PhysicsMaterial2DCreate(), true);
        }

        public float Density
        {
            get { return ManagedRuntimeContext.PhysicsMaterial2DGetDensity(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetDensity(m_ManagedUuid, value); }
        }

        public float Friction
        {
            get { return ManagedRuntimeContext.PhysicsMaterial2DGetFriction(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetFriction(m_ManagedUuid, value); }
        }

        public float Restitution
        {
            get { return ManagedRuntimeContext.PhysicsMaterial2DGetRestitution(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetRestitution(m_ManagedUuid, value); }
        }

        public float RestitutionThreshold
        {
            get { return ManagedRuntimeContext.PhysicsMaterial2DGetRestitutionThreshold(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetRestitutionThreshold(m_ManagedUuid, value); }
        }

        public PhysicsCombineMode FrictionCombine
        {
            get { return (PhysicsCombineMode)ManagedRuntimeContext.PhysicsMaterial2DGetFrictionCombine(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetFrictionCombine(m_ManagedUuid, (int)value); }
        }

        public PhysicsCombineMode RestitutionCombine
        {
            get { return (PhysicsCombineMode)ManagedRuntimeContext.PhysicsMaterial2DGetRestitutionCombine(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial2DSetRestitutionCombine(m_ManagedUuid, (int)value); }
        }
    }

    /// <summary>Reusable material assigned to a 3D collider.</summary>
    public sealed class PhysicsMaterial3D : Asset
    {
        /// <summary>Creates a runtime material shared by Box3D, Jolt, and Bullet colliders.</summary>
        public static PhysicsMaterial3D Create()
        {
            return ManagedRuntimeContext.CreateAsset<PhysicsMaterial3D>(ManagedRuntimeContext.PhysicsMaterial3DCreate(), true);
        }

        public float Density
        {
            get { return ManagedRuntimeContext.PhysicsMaterial3DGetDensity(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetDensity(m_ManagedUuid, value); }
        }

        public float Friction
        {
            get { return ManagedRuntimeContext.PhysicsMaterial3DGetFriction(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetFriction(m_ManagedUuid, value); }
        }

        public float Restitution
        {
            get { return ManagedRuntimeContext.PhysicsMaterial3DGetRestitution(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetRestitution(m_ManagedUuid, value); }
        }

        public float RestitutionThreshold
        {
            get { return ManagedRuntimeContext.PhysicsMaterial3DGetRestitutionThreshold(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetRestitutionThreshold(m_ManagedUuid, value); }
        }

        public PhysicsCombineMode FrictionCombine
        {
            get { return (PhysicsCombineMode)ManagedRuntimeContext.PhysicsMaterial3DGetFrictionCombine(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetFrictionCombine(m_ManagedUuid, (int)value); }
        }

        public PhysicsCombineMode RestitutionCombine
        {
            get { return (PhysicsCombineMode)ManagedRuntimeContext.PhysicsMaterial3DGetRestitutionCombine(m_ManagedUuid); }
            set { ManagedRuntimeContext.PhysicsMaterial3DSetRestitutionCombine(m_ManagedUuid, (int)value); }
        }
    }
}
