using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class MeshRenderer : Component
    {
        /// <summary>
        /// The material that is used for rendering this mesh.
        /// </summary>
        public Mesh mesh
        {
            get { return Internal_GetMesh(m_InternalPtr); }
            set { Internal_SetMesh(m_InternalPtr, value); }
        }

        /// <summary>
        /// The first material that is used for rendering this mesh.
        /// </summary>
        public Material material
        {
            get { return Internal_GetMaterial(m_InternalPtr, 0); }
            set { Internal_SetMaterial(m_InternalPtr, 0, value); }
        }

        /// <summary>
        /// The list of all materials used for the sub-meshes in the component.
        /// </summary>
        public Material[] materials
        {
            get { return Internal_GetMaterials(m_InternalPtr); }
            set { Internal_SetMaterials(m_InternalPtr, value); }
        }

        /// <summary>
        /// Sets the material for a sub-mesh at an index.
        /// </summary>
        /// <param name="idx">The index of the mateiral to set.</param>
        /// <param name="material">The material to use.</param>
        public void SetMaterial(int idx, Material material)
        {
            Internal_SetMaterial(m_InternalPtr, idx, material);
        }

        /// <summary>
        /// Retrieves the material used for rendering the sub-mesh at index idx.
        /// </summary>
        /// <param name="idx">The index of the mateiral to get.</param>
        /// <returns>The retrieved material or null if index is out of bounds.</returns>
        public Material GetMaterial(int idx)
        {
            return Internal_GetMaterial(m_InternalPtr, idx);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Mesh Internal_GetMesh(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMesh(IntPtr parent, Mesh mesh);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Material Internal_GetMaterial(IntPtr parent, int idx);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMaterial(IntPtr parent, int idx, Material material);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Material[] Internal_GetMaterials(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMaterials(IntPtr parent, Material[] material);

    }
}
