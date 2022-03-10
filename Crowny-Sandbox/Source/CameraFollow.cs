using System;
using System.Collections.Generic;

using Crowny;

namespace Sandbox
{
    public class CameraFollow : EntityBehaviour
    {
        public Entity target;
        public Vector3 offset = new Vector3(0, 0, 20);
        public float smoothSpeed = 0.125f;

        void Update()
        {
            Vector3 deseriedPos = target.transform.position + offset;
            // Vector3 smoothPos = Vector3.Lerp(transform.position, desiredPos, smoothSpeed);
            transform.position = deseriedPos;
        }
    }
}
