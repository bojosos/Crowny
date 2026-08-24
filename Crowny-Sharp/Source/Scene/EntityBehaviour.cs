namespace Crowny
{
    /// <summary>Base class for managed entity scripts.</summary>
    public class EntityBehaviour : Component
    {
        /// <summary>Called when this entity starts touching another 2D collider.</summary>
        protected virtual void OnCollisionEnter2D(Collision2D collision) { }

        /// <summary>Called during each physics step while a 2D collision persists.</summary>
        protected virtual void OnCollisionStay2D(Collision2D collision) { }

        /// <summary>Called when this entity stops touching another 2D collider.</summary>
        protected virtual void OnCollisionExit2D(Collision2D collision) { }

        /// <summary>Called when another entity enters this entity's 2D trigger.</summary>
        protected virtual void OnTriggerEnter2D(Entity other) { }

        /// <summary>Called during each physics step while an entity remains in this 2D trigger.</summary>
        protected virtual void OnTriggerStay2D(Entity other) { }

        /// <summary>Called when another entity exits this entity's 2D trigger.</summary>
        protected virtual void OnTriggerExit2D(Entity other) { }

        /// <summary>Called when this entity starts touching another 3D collider.</summary>
        protected virtual void OnCollisionEnter3D(Collision3D collision) { }

        /// <summary>Called during each physics step while a 3D collision persists.</summary>
        protected virtual void OnCollisionStay3D(Collision3D collision) { }

        /// <summary>Called when this entity stops touching another 3D collider.</summary>
        protected virtual void OnCollisionExit3D(Collision3D collision) { }

        /// <summary>Called when another entity enters this entity's 3D trigger.</summary>
        protected virtual void OnTriggerEnter3D(Entity other) { }

        /// <summary>Called during each physics step while an entity remains in this 3D trigger.</summary>
        protected virtual void OnTriggerStay3D(Entity other) { }

        /// <summary>Called when another entity exits this entity's 3D trigger.</summary>
        protected virtual void OnTriggerExit3D(Entity other) { }
    }

    internal sealed class MissingEntityBehaviour : EntityBehaviour
    {
    }
}
