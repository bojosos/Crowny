using Crowny.Math;

namespace Crowny
{
    class Transform
    {
        // Position of the transform in world space
        public extern Vector3 position { get; set; }

        // Position of the transform relative to its parent
        public extern Vector3 localPosition { get; set; }

        // Rotation of the transform
        //public extern Quaternion rotation { get; set; }

        // Scale of the transform relative to its parent
        public extern Vector3 localScale { get; set; }

        // Rotation of the transform in degrees
        public extern Vector3 eulerAngles { get; set; }

        // Rotation of the transform in degrees relative to its parent
        public extern Vector3 localEulerAngles { get; set; }



    }
}
