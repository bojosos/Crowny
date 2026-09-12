using System;
using Crowny;

namespace Sandbox
{
    /// <summary>Exercises script and collision object lifetimes through forced collections.</summary>
    public class MonoGcLifetimeProbe : EntityBehaviour
    {
        /// <summary>Value written after collecting inside the constructor.</summary>
        public int ConstructorValue;
        /// <summary>Number of completed update callbacks.</summary>
        public int UpdateCount;
        /// <summary>Number of completed collision callbacks.</summary>
        public int CollisionCount;

        /// <summary>Collects while the native caller is constructing this instance.</summary>
        public MonoGcLifetimeProbe()
        {
            Collect();
            ConstructorValue = 73;
        }

        private static void Collect()
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }

        private void Update()
        {
            Collect();
            UpdateCount++;
        }

        private void OnCollisionEnter2D(Collision2D collision)
        {
            Collect();
            if (collision.Colliders == null || collision.Colliders.Length != 2 ||
                collision.Colliders[0] == null || collision.Colliders[1] == null ||
                collision.ContactCount != 1 || collision.GetContact(0).Point.x != 4.0f)
                throw new InvalidOperationException("2D collision references did not survive collection.");
            CollisionCount++;
        }

        private void OnCollisionEnter3D(Collision3D collision)
        {
            Collect();
            if (collision.Colliders == null || collision.Colliders.Length != 2 ||
                collision.Colliders[0] == null || collision.Colliders[1] == null ||
                collision.ContactCount != 1 || collision.GetContact(0).Point.x != 4.0f)
                throw new InvalidOperationException("3D collision references did not survive collection.");
            CollisionCount++;
        }
    }
}
