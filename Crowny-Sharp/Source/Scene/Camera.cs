namespace Crowny
{
    public enum CameraProjection
    {
        Orthographic,
        Perspective
    }

    public class Camera : Component
    {
        /// <summary>
        /// The vertical field of view of the camera, in degrees.
        /// </summary>
        /// <value>Camera field of view.</value>
        public float fieldOfView
        {
            get { return ManagedRuntimeContext.CameraGetFieldOfView(EntityId); }
            set { ManagedRuntimeContext.CameraSetFieldOfView(EntityId, value); }
        }

        /// <summary>The projection used by this camera.</summary>
        public CameraProjection projection
        {
            get { return (CameraProjection)ManagedRuntimeContext.CameraGetProjection(EntityId); }
            set { ManagedRuntimeContext.CameraSetProjection(EntityId, (int)value); }
        }

        /// <summary>Whether this camera uses an orthographic projection.</summary>
        public bool orthographic
        {
            get { return projection == CameraProjection.Orthographic; }
            set { projection = value ? CameraProjection.Orthographic : CameraProjection.Perspective; }
        }

        /// <summary>
        /// The near clip plane of the camera.
        /// </summary>
        /// <value>Camera near clip plane.</value>
        public float nearClipPlane
        {
            get { return ManagedRuntimeContext.CameraGetNearClipPlane(EntityId); }
            set { ManagedRuntimeContext.CameraSetNearClipPlane(EntityId, value); }
        }

        /// <summary>
        /// The far clip of the camera.
        /// </summary>
        /// <value>Camera far clip.</value>
        public float farClipPlane
        {
            get { return ManagedRuntimeContext.CameraGetFarClipPlane(EntityId); }
            set { ManagedRuntimeContext.CameraSetFarClipPlane(EntityId, value); }
        }

        /// <summary>
        /// The orthographic size of the camera, when using orthographic projection.
        /// </summary>
        /// <value>Camera orthographic size.</value>
        public float orthographicSize
        {
            get { return ManagedRuntimeContext.CameraGetOrthographicSize(EntityId); }
            set { ManagedRuntimeContext.CameraSetOrthographicSize(EntityId, value); }
        }

        /// <summary>
        /// The aspect ratio of the camera when using orthographic projection.
        /// </summary>
        /// <value>Camera aspect ratio.</value>
        public float aspectRatio
        {
            get { return ManagedRuntimeContext.CameraGetAspectRatio(EntityId); }
            set { ManagedRuntimeContext.CameraSetAspectRatio(EntityId, value); }
        }

        /// <summary>
        /// The clear color used when rendering.
        /// </summary>
        /// <value></value>
        public Vector3 backgroundColor
        {
            get
            {
                return ManagedRuntimeContext.CameraGetBackgroundColor(EntityId);
            }
            set { ManagedRuntimeContext.CameraSetBackgroundColor(EntityId, value); }
        }

        /// <summary>
        /// The size and position of the rectangle the camera is rendering to on the screen. 
        /// </summary>
        /// <value>All four values are in the range [0, 1].</value>
        public Vector4 viewportRectangle
        {
            get
            {
                return ManagedRuntimeContext.CameraGetViewportRectangle(EntityId);
            }
            set { ManagedRuntimeContext.CameraSetViewportRectangle(EntityId, value); }
        }

        /// <summary>Whether the camera renders into a high-dynamic-range target.</summary>
        public bool hdr
        {
            get { return ManagedRuntimeContext.CameraGetHdr(EntityId); }
            set { ManagedRuntimeContext.CameraSetHdr(EntityId, value); }
        }

        /// <summary>Whether multisample anti-aliasing is enabled for this camera.</summary>
        public bool msaa
        {
            get { return ManagedRuntimeContext.CameraGetMsaa(EntityId); }
            set { ManagedRuntimeContext.CameraSetMsaa(EntityId, value); }
        }

        /// <summary>Whether occlusion culling is enabled for this camera.</summary>
        public bool occlusionCulling
        {
            get { return ManagedRuntimeContext.CameraGetOcclusionCulling(EntityId); }
            set { ManagedRuntimeContext.CameraSetOcclusionCulling(EntityId, value); }
        }
    }
}
