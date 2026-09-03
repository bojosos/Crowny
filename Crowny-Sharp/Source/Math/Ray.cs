namespace Crowny
{
    /// <summary>A ray in world space, defined by an origin point and a normalized direction.</summary>
    public struct Ray
    {
        /// <summary>The starting point of the ray in world space.</summary>
        public Vector3 Origin;

        /// <summary>The normalized direction of the ray in world space.</summary>
        public Vector3 Direction;

        /// <summary>Creates a ray. The direction is normalized.</summary>
        /// <param name="origin">The starting point of the ray.</param>
        /// <param name="direction">The direction of the ray. Does not need to be normalized.</param>
        public Ray(Vector3 origin, Vector3 direction)
        {
            Origin = origin;
            Direction = direction.normalized;
        }

        /// <summary>Returns a point on the ray at the given distance from the origin.</summary>
        /// <param name="distance">The distance from the origin along the direction.</param>
        public Vector3 GetPoint(float distance)
        {
            return Origin + Direction * distance;
        }

        public override string ToString()
        {
            return "Origin: " + Origin + " Dir: " + Direction;
        }
    }
}
