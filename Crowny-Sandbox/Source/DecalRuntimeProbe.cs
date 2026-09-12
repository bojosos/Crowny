using System;
using Crowny;

namespace Sandbox
{
    /// <summary>Exercises the decal ABI from both supported managed runtimes.</summary>
    public class DecalRuntimeProbe : EntityBehaviour
    {
        private int stage;

        private void Start()
        {
            DecalComponent decal = entity.AddComponent<DecalComponent>();
            decal.Projection = DecalProjection.Cylinder;
            decal.BottomRadius = 0.7f;
            decal.TopRadius = 0.4f;
            decal.Height = 2;
            decal.Opacity = 0.6f;
            decal.SortOrder = -11;
            decal.UVScale = new Vector2(-2, 3);
            decal.UVOffset = new Vector2(0.1f, 0.2f);
            decal.TargetMode = DecalTargetMode.Entity;
            decal.Target = entity.uuid;
            decal.Lifetime = 4;
            decal.FadeOut = 1;
            decal.Material = null;
            if (decal.Projection != DecalProjection.Cylinder || decal.TopRadius != 0.4f ||
                decal.SortOrder != -11 || decal.Target != entity.uuid || decal.Material != null)
                throw new InvalidOperationException("Decal property round trip failed.");
        }

        private void Update()
        {
            DecalComponent decal = entity.GetComponent<DecalComponent>();
            if (stage == 0) decal.StopLifetime();
            else if (stage == 1) decal.RestartLifetime();
            else
            {
                entity.RemoveComponent<DecalComponent>();
                if (entity.HasComponent<DecalComponent>())
                    throw new InvalidOperationException("Removed decal remains registered.");
            }
            ++stage;
        }
    }
}
