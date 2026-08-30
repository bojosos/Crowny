using System;

namespace Crowny
{
    public class MeshRenderer : Component
    {
        /// <summary>The mesh rendered by this component.</summary>
        public Mesh mesh
        {
            get { return ManagedRuntimeContext.CreateAsset<Mesh>(ManagedRuntimeContext.MeshRendererGetMesh(EntityId)); }
            set { ManagedRuntimeContext.MeshRendererSetMesh(EntityId, value != null ? value.uuid : UUID.Empty); }
        }

        /// <summary>The default material used by this component.</summary>
        public Material material
        {
            get { return GetMaterial(0); }
            set { SetMaterial(0, value); }
        }

        /// <summary>The materials used for the component's sub-meshes.</summary>
        public Material[] materials
        {
            get
            {
                uint count = ManagedRuntimeContext.MeshRendererGetMaterialCount(EntityId);
                Material[] result = new Material[checked((int)count)];
                for (uint index = 0; index < count; ++index)
                    result[index] = ManagedRuntimeContext.CreateAsset<Material>(
                        ManagedRuntimeContext.MeshRendererGetMaterial(EntityId, index));
                return result;
            }
            set
            {
                if (value == null)
                    throw new ArgumentNullException("value");
                ManagedRuntimeContext.MeshRendererSetMaterialCount(EntityId, checked((uint)value.Length));
                for (uint index = 0; index < (uint)value.Length; ++index)
                {
                    Material current = value[index];
                    ManagedRuntimeContext.MeshRendererSetMaterial(EntityId, index,
                        current != null ? current.uuid : UUID.Empty);
                }
            }
        }

        /// <summary>Sets the material for one sub-mesh.</summary>
        public void SetMaterial(int idx, Material material)
        {
            if (idx < 0)
                throw new ArgumentOutOfRangeException("idx");
            ManagedRuntimeContext.MeshRendererSetMaterial(EntityId, checked((uint)idx),
                material != null ? material.uuid : UUID.Empty);
        }

        /// <summary>Gets a sub-mesh material, falling back to the default material when needed.</summary>
        public Material GetMaterial(int idx)
        {
            if (idx < 0)
                throw new ArgumentOutOfRangeException("idx");
            return ManagedRuntimeContext.CreateAsset<Material>(
                ManagedRuntimeContext.MeshRendererGetMaterial(EntityId, checked((uint)idx)));
        }
    }
}
