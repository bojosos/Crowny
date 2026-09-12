using Crowny;

namespace Sandbox
{
    // The native regression tests shrink the script vector before dispatch so adding this component relocates it.
    public sealed class ScriptMutationProbe : EntityBehaviour
    {
        protected override void OnTriggerEnter2D(Entity other) { Mutate(); }
        protected override void OnTriggerEnter3D(Entity other) { Mutate(); }

        private void Mutate()
        {
            if (!entity.HasComponent<ScriptMutationAdded>())
                entity.AddComponent<ScriptMutationAdded>();
            if (entity.HasComponent<ScriptMutationRemoved>())
                entity.RemoveComponent<ScriptMutationRemoved>();
            entity.name += "mutator|";
        }
    }

    public sealed class ScriptMutationRemoved : EntityBehaviour
    {
        protected override void OnTriggerEnter2D(Entity other) { entity.name += "removed|"; }
        protected override void OnTriggerEnter3D(Entity other) { entity.name += "removed|"; }
    }

    public sealed class ScriptMutationSurvivor : EntityBehaviour
    {
        protected override void OnTriggerEnter2D(Entity other) { entity.name += "survivor|"; }
        protected override void OnTriggerEnter3D(Entity other) { entity.name += "survivor|"; }
    }

    public sealed class ScriptMutationAdded : EntityBehaviour
    {
        protected override void OnTriggerEnter2D(Entity other) { entity.name += "added|"; }
        protected override void OnTriggerEnter3D(Entity other) { entity.name += "added|"; }
    }

    public sealed class ScriptDestroyMutationProbe : EntityBehaviour
    {
        private void OnDestroy()
        {
            entity.name += "destroy|";
            entity.AddComponent<ScriptMutationAdded>();
            entity.RemoveComponent<ScriptDestroyMutationProbe>();
        }
    }
}
